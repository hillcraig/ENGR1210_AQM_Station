#include <Arduino.h>
#include <Notecard.h>
#include <Adafruit_PM25AQI.h> 
#include <Adafruit_INA260.h> 
#include <Adafruit_AHTX0.h> 

#define productUID "edu.umn.d.cshill:engr_1210_fall_2024"  // Product UID for Notecard

// Object declaration for the Notecard
Notecard notecard;

// Function prototypes
void Notecard_Find_Location();
void Send_Data();
void Set_Time_Location(J *rsp);

// Variables to store time and location data
char yyyy[5];
char mM[3];
char dd[3];
char hh[3];
char mm[3];
char ss[3];
double lat;
double lon;

void setup()
{
  delay(2000);  // Initial delay to allow peripherals to stabilize
  Serial.begin(115200);  // Initialize serial for debugging

  notecard.setDebugOutputStream(Serial);  // Set Notecard to output debug info over serial

  notecard.begin();  // Initialize the Notecard

  // Set up the Notecard for hub communication
  {  
    J *req = notecard.newRequest("hub.set");
    JAddStringToObject(req, "product", productUID);
    JAddStringToObject(req, "mode", "periodic");  // Periodic communication mode
    JAddNumberToObject(req, "inbound", 60 * 12);  // Set inbound interval (12 minutes)
    JAddNumberToObject(req, "outbound", 30);  // Set outbound interval (30 seconds)
    if (!notecard.sendRequest(req)) {
      JDelete(req);  // Delete the request if sending fails
    }
  }

  // Enable DFU (Device Firmware Update) mode on the Notecard
  {
    J *req = notecard.newRequest("card.aux");
    JAddStringToObject(req, "mode", "dfu");  // Set auxiliary card mode to DFU
    if (!notecard.sendRequest(req)) {
      JDelete(req);  // Delete the request if sending fails
    }
  }

  // Set up periodic location tracking (every 5 minutes)
  {
    J *req = notecard.newRequest("card.location.mode");
    JAddStringToObject(req, "mode", "periodic");
    JAddNumberToObject(req, "seconds", 60 * 5);  // Set location interval to 5 minutes
    if (!notecard.sendRequest(req)) {
      JDelete(req);  // Delete the request if sending fails
    }
  }
}

void loop()
{
  unsigned long startTime = millis();  // Record the start time

  // Execute the tasks: location and data sending
  Notecard_Find_Location();
  Send_Data();

  // Spin the CPU until 5 minutes (300,000 ms) have passed
  while (millis() - startTime < 300000)
  {
    // Busy-wait to ensure the loop runs for exactly 5 minutes
  } // Change to 15 minutes for deployment. 
}

void Notecard_Find_Location()
{
  size_t gps_time_s;

  // Fetch the current location from the Notecard
  {
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.location"));
    gps_time_s = JGetInt(rsp, "time");  // Get the GPS time
    NoteDeleteResponse(rsp);
  }

  // Switch the Notecard to continuous location tracking mode
  {
    J *req = notecard.newRequest("card.location.mode");
    JAddStringToObject(req, "mode", "continuous");
    notecard.sendRequest(req);
  }

  size_t timeout_s = 600;  // 10-minute timeout for finding a location

  // Poll for updated location data
  for (const size_t start_ms = ::millis();;) {
    if (::millis() >= (start_ms + (timeout_s * 1000))) {
      Serial.println("Timed out looking for a location\n");
      break;  // Timeout reached, stop looking for location
    }
  
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.location"));
    if (JGetInt(rsp, "time") != gps_time_s) {  // Check if GPS time has updated
      Set_Time_Location(rsp);  // If updated, set the time and location
      NoteDeleteResponse(rsp);

      // Switch the Notecard back to periodic location mode
      {
        J *req = notecard.newRequest("card.location.mode");
        JAddStringToObject(req, "mode", "periodic");
        notecard.sendRequest(req);
      }
      break;
    }
  
    if (JGetObjectItem(rsp, "stop")) {  // If stop flag is found, break out
      Serial.println("Found a stop flag, cannot find location\n");
      break;
    }
  
    NoteDeleteResponse(rsp);
    delay(2000);  // Wait 2 seconds between location requests
  }
}

void Set_Time_Location(J *rsp)
{
  // Parse and set the time and location from the Notecard response
  time_t rawtime;
  rawtime = JGetNumber(rsp, "time");  

  lon = JGetNumber(rsp, "lon"); 
  lon = (floor(100000*lon)/100000);  // Truncate longitude to 5 decimal places
  lat = JGetNumber(rsp, "lat");
  lat = (floor(100000*lat)/100000);  // Truncate latitude to 5 decimal places

  struct tm  ts;  
  ts = *localtime(&rawtime);  // Convert raw time to local time
  strftime(yyyy, sizeof(yyyy), "%Y", &ts); 
  strftime(mM, sizeof(mM), "%m", &ts); 
  strftime(dd, sizeof(dd), "%d", &ts); 
  strftime(hh, sizeof(hh), "%H", &ts); 
  strftime(ss, sizeof(ss), "%S", &ts); 
  strftime(mm, sizeof(mm), "%M", &ts); 
}

void Send_Data()
{
  // Create a Notecard request to send data (date/time and extra info)
  J *req = notecard.newRequest("note.add");  
  if (req != NULL)
  {
    JAddStringToObject(req, "file", "data.qo");  // Store data in "data.qo" file
    JAddBoolToObject(req, "sync", true);  // Request immediate sync
    J *body = JAddObjectToObject(req, "body");
    if (body)
    {
      // Add time data
      JAddStringToObject(body, "YYYY", yyyy);
      JAddStringToObject(body, "MM", mM);
      JAddStringToObject(body, "DD", dd);  
      JAddStringToObject(body, "hh", hh);
      JAddStringToObject(body, "mm", mm);
      JAddStringToObject(body, "ss", ss);

      
      JAddNumberToObject(body, "lat", lat);  
      JAddNumberToObject(body, "lon", lon);  
    }

    notecard.sendRequest(req);  // Send the request to the Notecard
  }
}