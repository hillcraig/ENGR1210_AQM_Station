
#include <Arduino.h>
#include <time.h> 
#include <Notecard.h>
#include <Adafruit_PM25AQI.h> 
#include <Adafruit_INA260.h> 
#include <Adafruit_AHTX0.h> 

#define productUID "edu.umn.d.cshill:engr_1210_fall_2024y"  // Product UID for Notecard

// Object declarations for the Notecard and sensors
Notecard notecard;
Adafruit_AHTX0 aht;
Adafruit_PM25AQI aqi;
Adafruit_INA260 ina260;

// Function prototypes
void Notecard_Find_Location();
void Read_AHTX0();
void Read_INA260();
void Read_PM25AQI();
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

// Variables to store sensor readings
float temperature;
float humidity;

float current;
float voltage;
float power;

float pm10;
float pm25;
float pm100;

void setup()
{
  delay(2000);  // Initial delay to allow peripherals to stabilize
  Serial.begin(115200);  // Initialize serial for debugging

  notecard.setDebugOutputStream(Serial);  // Set Notecard to output debug info over serial

  // Initialize the AHTX0 sensor (Temperature & Humidity)
  if (! aht.begin()) {
    Serial.println("Could not find AHTX0 sensor!");
    while (1);  // Stop the program if the sensor is not found
  }
  Serial.println("AHTX0 found!");

  // Initialize the PM2.5 AQI sensor (Air Quality)
  if (! aqi.begin_I2C()) {     
    Serial.println("Could not find PM 2.5 sensor!");
    while (1);  // Stop the program if the sensor is not found
  }
  Serial.println("PM25 found!");

  // Initialize the INA260 sensor (Power, Current, Voltage)
  if (!ina260.begin()) {
    Serial.println("Couldn't find INA260 sensor!");
    while (1);  // Stop the program if the sensor is not found
  }
  Serial.println("INA260 found!");

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

  // Execute the tasks: location and sensor readings
  Notecard_Find_Location();
  Read_AHTX0();
  Read_INA260();
  Read_PM25AQI();
  Send_Data();

  // Spin the CPU until 15 minutes (900,000 ms) have passed
  while (millis() - startTime < 900000)
  {
    // Busy-wait to ensure the loop runs for exactly 15 minutes
  }
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

void Read_AHTX0()
{
  // Read temperature and humidity from AHTX0 sensor
  sensors_event_t humid, temp;
  aht.getEvent(&humid, &temp);
  temperature = temp.temperature;  // Store temperature value
  humidity = humid.relative_humidity;  // Store humidity value
}

void Read_INA260()
{
  // Read current, voltage, and power from INA260 sensor
  current = ina260.readCurrent();
  voltage = ina260.readBusVoltage();
  power = ina260.readPower(); 
}

void Read_PM25AQI()
{
  PM25_AQI_Data data;
  
  aqi.read(&data);  // Read air quality data from PM2.5 AQI sensor

  pm10 = data.pm10_standard;  // PM10 concentration
  pm25 = data.pm25_standard;  // PM2.5 concentration
  pm100 = data.pm100_standard;  // PM100 concentration
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
  // Create a Notecard request to send sensor data
  J *req = notecard.newRequest("note.add");  
  if (req != NULL)
  {
    JAddStringToObject(req, "file", "sensors.qo");  // Store data in "sensors.qo" file
    JAddBoolToObject(req, "sync", true);  // Request immediate sync
    J *body = JAddObjectToObject(req, "body");
    if (body)
    {
      // Add time and location data
      JAddStringToObject(body, "YYYY", yyyy);
      JAddStringToObject(body, "MM", mM);
      JAddStringToObject(body, "DD", dd);  
      JAddStringToObject(body, "hh", hh);
      JAddStringToObject(body, "mm", mm);
      JAddStringToObject(body, "ss", ss);
      JAddNumberToObject(body, "lat", lat);            
      JAddNumberToObject(body, "lon", lon);   

      // Add sensor data
      JAddNumberToObject(body, "temp", temperature);  // Add temperature data
      JAddNumberToObject(body, "humidity", humidity);  // Add humidity data

      // Add PM2.5 sensor data
      JAddNumberToObject(body, "pm10", pm10);  // Add PM10 data
      JAddNumberToObject(body, "pm25", pm25);  // Add PM2.5 data
      JAddNumberToObject(body, "pm100", pm100);  // Add PM100 data

      // Add INA260 sensor data (current, voltage, power)
      JAddNumberToObject(body, "current", current);  // Add current data
      JAddNumberToObject(body, "voltage", voltage);  // Add voltage data
      JAddNumberToObject(body, "power", power);  // Add power data
    }

    notecard.sendRequest(req);  // Send the request to the Notecard
  }
}