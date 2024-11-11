
#include <Arduino.h>
#include <time.h> 
#include <Notecard.h>
#include <Adafruit_PM25AQI.h> 
#include <Adafruit_INA260.h> 
#include <Adafruit_AHTX0.h> 

#define productUID "edu.umn.d.cshill:engr_1210_fall_2024"  // Product UID for Notecard

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

// PM2.5 AQI sensor variables for standard and environmental particulate concentrations
float pm10_standard, pm25_standard, pm100_standard;
float pm10_env, pm25_env, pm100_env;

// Variables to store particle counts
uint16_t particles_03um;
uint16_t particles_05um;
uint16_t particles_10um;
uint16_t particles_25um;
uint16_t particles_50um;
uint16_t particles_100um;

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

  // Set INA260 to average over 16 samples
  ina260.setAveragingCount(INA260_COUNT_16);

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

  // Spin the CPU until 5 minutes (300,000 ms) have passed
  while (millis() - startTime < 300000)
  {
    // Busy-wait to ensure the loop runs for exactly 5 minutes
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

  // Read air quality data from the PM2.5 AQI sensor
  if (aqi.read(&data)) {
    // Standard particulate matter concentrations
    pm10_standard = data.pm10_standard;  // PM10 (standard)
    pm25_standard = data.pm25_standard;  // PM2.5 (standard)
    pm100_standard = data.pm100_standard;  // PM100 (standard)

    // Environmental particulate matter concentrations
    pm10_env = data.pm10_env;  // PM10 (environmental)
    pm25_env = data.pm25_env;  // PM2.5 (environmental)
    pm100_env = data.pm100_env;  // PM100 (environmental)

    // Particle counts for different sizes
    particles_03um = data.particles_03um;  // Particles > 0.3um / 0.1L air
    particles_05um = data.particles_05um;  // Particles > 0.5um / 0.1L air
    particles_10um = data.particles_10um;  // Particles > 1.0um / 0.1L air
    particles_25um = data.particles_25um;  // Particles > 2.5um / 0.1L air
    particles_50um = data.particles_50um;  // Particles > 5.0um / 0.1L air
    particles_100um = data.particles_100um;  // Particles > 50um / 0.1L air
  } else {
    Serial.println("Failed to read from PM2.5 sensor!");
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
  // Create a Notecard request to send sensor data
  J *req = notecard.newRequest("note.add");  
  if (req != NULL)
  {
    JAddStringToObject(req, "file", "data.qo");  // Store data in "sensors.qo" file
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

      // Add sensor data for temperature and humidity
      JAddNumberToObject(body, "temperature", temperature);  // Temperature
      JAddNumberToObject(body, "humidity", humidity);  // Humidity

      // Add PM2.5 AQI sensor data
      JAddNumberToObject(body, "pm10_standard", pm10_standard);  // PM10 (standard)
      JAddNumberToObject(body, "pm25_standard", pm25_standard);  // PM2.5 (standard)
      JAddNumberToObject(body, "pm100_standard", pm100_standard);  // PM100 (standard)
      JAddNumberToObject(body, "pm10_env", pm10_env);  // PM10 (environmental)
      JAddNumberToObject(body, "pm25_env", pm25_env);  // PM2.5 (environmental)
      JAddNumberToObject(body, "pm100_env", pm100_env);  // PM100 (environmental)

      // Add particle counts for various sizes
      JAddNumberToObject(body, "particles_03um", particles_03um);  // Particles > 0.3um
      JAddNumberToObject(body, "particles_05um", particles_05um);  // Particles > 0.5um
      JAddNumberToObject(body, "particles_10um", particles_10um);  // Particles > 1.0um
      JAddNumberToObject(body, "particles_25um", particles_25um);  // Particles > 2.5um
      JAddNumberToObject(body, "particles_50um", particles_50um);  // Particles > 5.0um
      JAddNumberToObject(body, "particles_100um", particles_100um);  // Particles > 50um

      // Add INA260 sensor data (current, voltage, power)
      JAddNumberToObject(body, "current", current);  // Current
      JAddNumberToObject(body, "voltage", voltage);  // Voltage
      JAddNumberToObject(body, "power", power);  // Power
    }

    notecard.sendRequest(req);  // Send the request to the Notecard
  }
}