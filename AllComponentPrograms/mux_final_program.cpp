
#include <Arduino.h>
#include <time.h> 
#include <Notecard.h>
#include <Adafruit_PM25AQI.h>   // Air Quality Sensor
#include <Adafruit_INA260.h>   // Voltage, Current, Power Sensor
#include <Adafruit_AHTX0.h>   // Air Temperature and Humidity Sensor
#include <SparkFun_I2C_Mux_Arduino_Library.h>

#define productUID "edu.umn.d.cshill:engr_1210_fall_2024"  // Product UID for Notecard

#define DEBUG 0

#define INA260_MUX_PORT 0
#define AHTX0_MUX_PORT 1
#define PM25AQI_MUX_PORT 2

// Object declarations for the Notecard and sensors
Notecard notecard;
Adafruit_AHTX0 aht;
Adafruit_PM25AQI aqi;
Adafruit_INA260 ina260;
QWIICMUX myMux;

// Function prototypes
void Notecard_Find_Location();
void Read_AHTX0();
void Read_INA260();
void Read_PM25AQI();
void Send_Data();
void Set_Time_Location(J *rsp);
void SetNotecardToOffMode();
template <typename T>
void debugPrint(T message);
template <typename T>
void debugPrintln(T message);

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

  Wire.begin();

  // Initialize the MUX
  if (myMux.begin() == false)
  {
    debugPrintln("Mux not detected. Freezing...");
    while (1);
  }
  debugPrintln("Mux detected");

  // Initialize the AHTX0 sensor (Temperature & Humidity)
  myMux.setPort(AHTX0_MUX_PORT);
  if (! aht.begin()) {
    debugPrintln("Could not find AHTX0 sensor!");
    while (1);  // Stop the program if the sensor is not found
  }
  debugPrintln("AHTX0 found!");

  // Initialize the PM2.5 AQI sensor (Air Quality)
  myMux.setPort(PM25AQI_MUX_PORT);
  if (! aqi.begin_I2C()) {     
    debugPrintln("Could not find PM 2.5 sensor!");
    while (1);  // Stop the program if the sensor is not found
  }
  debugPrintln("PM25 found!");

  // Initialize the INA260 sensor (Power, Current, Voltage)
  myMux.setPort(INA260_MUX_PORT);
  if (!ina260.begin()) {
    debugPrintln("Couldn't find INA260 sensor!");
    while (1);  // Stop the program if the sensor is not found
  }
  debugPrintln("INA260 found!");

  // Set INA260 to average over 16 samples
  ina260.setAveragingCount(INA260_COUNT_16);

  notecard.begin(Serial1);  // Initialize the Notecard in UART mode
  
  #if DEBUG
  notecard.setDebugOutputStream(Serial);  // Set Notecard to output debug info over serial
  #endif

  // Set up the Notecard for hub communication
  {  
    J *req = notecard.newRequest("hub.set");
    JAddStringToObject(req, "product", productUID);
    JAddStringToObject(req, "mode", "periodic");  // periodic communication mode
    if (!notecard.sendRequest(req)) {
      JDelete(req);  // Delete the request if sending fails
    }
  }

  // Perform an immediate sync to fetch the current time and update settings
  {
    J *req = notecard.newRequest("hub.sync");
    if (!notecard.sendRequest(req)) {
      debugPrintln("Failed to perform initial hub sync\n");
    }
  }

}

void loop()
{
  // Fetch the Notecard's current time
  unsigned long notecardTime = 0;
  {
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.time"));
    if (rsp != NULL) {
      notecardTime = JGetInt(rsp, "time");  // Get the Notecard's current timestamp (UTC)
      NoteDeleteResponse(rsp);
    } else {
      debugPrintln("Failed to get Notecard time. Retrying...\n");
      delay(1000);  // Retry after a delay
      return;
    }
  }

  debugPrintln("Waiting until the next 15-minute mark.");
  // Calculate seconds until the next 15-minute mark
  unsigned long secondsUntilNextMark = (900 - (notecardTime % 900));  // 900 seconds = 15 minutes

  // Busy wait until the next 15-minute mark
  unsigned long startWaitTime = millis();  // Record the start time of waiting
  unsigned long waitTimeMs = secondsUntilNextMark * 1000;  // Convert to milliseconds
  while (millis() - startWaitTime < waitTimeMs) {
    // Actively wait for the duration to pass
  }
  debugPrintln("Reached the 15-minute mark. Starting tasks.");

  // Execute tasks on the exact mark
  myMux.setPort(INA260_MUX_PORT);
  Read_INA260();
  myMux.setPort(PM25AQI_MUX_PORT);
  Read_PM25AQI();
  myMux.setPort(AHTX0_MUX_PORT);
  Read_AHTX0();
  Notecard_Find_Location();
  Send_Data();
}

void Notecard_Find_Location()
{
  size_t gps_time_s = 0;
  const size_t timeout_s = 600;  // 10-minute timeout for finding a location

  // Fetch the current location time
  {
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.location"));
    if (rsp != NULL) {
      gps_time_s = JGetInt(rsp, "time");  // Get the GPS time
      NoteDeleteResponse(rsp);
    } else {
      debugPrintln("Failed to fetch initial location time\n");
      return;  // Exit if we can't fetch the initial GPS time
    }
  }

  // Switch to continuous location tracking mode
  {
    J *req = notecard.newRequest("card.location.mode");
    if (req != NULL) {
      JAddStringToObject(req, "mode", "continuous");
      if (!notecard.sendRequest(req)) {
        debugPrintln("Failed to switch to continuous mode\n");
        return;  // Exit if the mode change fails
      }
    }
  }

  // Poll for updated location data
  for (const size_t start_ms = ::millis();;) {
    // Check for timeout
    if (::millis() >= (start_ms + (timeout_s * 1000))) {
      debugPrintln("Timed out looking for a location\n");

      // Set time before exiting
      J *rsp = notecard.requestAndResponse(notecard.newRequest("card.time"));
      if (rsp != NULL) {
        Set_Time_Location(rsp);
        NoteDeleteResponse(rsp);
      }

      SetNotecardToOffMode();  // Ensure system is returned to off mode
      break;
    }

    // Fetch current location data
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.location"));
    if (rsp != NULL) {
      if (JGetInt(rsp, "time") != gps_time_s) {
        // Location updated, process new data
        Set_Time_Location(rsp);
        NoteDeleteResponse(rsp);

        SetNotecardToOffMode();  // Ensure system is returned to off mode
        break;  // Exit loop once location is updated
      }

      if (JGetObjectItem(rsp, "stop")) {  // Check for a "stop" flag
        debugPrintln("Found a stop flag, cannot find location\n");
        NoteDeleteResponse(rsp);

        SetNotecardToOffMode();  // Ensure system is returned to off mode
        break;
      }

      NoteDeleteResponse(rsp);  // Clean up response
    }

    delay(2000);  // Wait 2 seconds before polling again
  }
}

void SetNotecardToOffMode() {
  J *req = notecard.newRequest("card.location.mode");
  if (req != NULL) {
    JAddStringToObject(req, "mode", "off");
    if (!notecard.sendRequest(req)) {
      debugPrintln("Failed to set Notecard to off mode\n");
    }
  } else {
    debugPrintln("Failed to create off mode request\n");
  }
}

void Read_AHTX0()
{
  float temperatureSum = 0;
  float humiditySum = 0;
  const int numReadings = 10;

  for (int i = 0; i < numReadings; i++)
  {
    // Read temperature and humidity from AHTX0 sensor
    sensors_event_t humid, temp;
    aht.getEvent(&humid, &temp);

    temperatureSum += temp.temperature;
    humiditySum += humid.relative_humidity;

    // Wait 500ms before the next reading
    delay(500);
  }

  // Calculate averages
  float temperatureAvg = temperatureSum / numReadings;
  float humidityAvg = humiditySum / numReadings;

  // Store the averaged values in global variables
  temperature = temperatureAvg;
  temperature = round(temperature * pow(10, 2)) / pow(10, 2);  // Truncate longitude to 2 decimal places
  humidity = humidityAvg;
  humidity = round(humidity * pow(10, 2)) / pow(10, 2);  // Truncate longitude to 2 decimal places

  // Debug output
  debugPrint("Averaged Temperature: ");
  debugPrintln(temperature);
  debugPrint("Averaged Humidity: ");
  debugPrintln(humidity);
}

void Read_INA260()
{
  float currentSum = 0;
  float voltageSum = 0;
  float powerSum = 0;
  const int numReadings = 10;

  for (int i = 0; i < numReadings; i++)
  {
    // Read current, voltage, and power from INA260 sensor
    currentSum += ina260.readCurrent();
    voltageSum += ina260.readBusVoltage();
    powerSum += ina260.readPower();

    // Wait 500ms before the next reading
    delay(500);
  }

  // Calculate averages
  float currentAvg = currentSum / numReadings;
  float voltageAvg = voltageSum / numReadings;
  float powerAvg = powerSum / numReadings;

  // Optionally store the averaged values in global variables or print them
  current = currentAvg;
  voltage = voltageAvg;
  power = powerAvg;

  // Debug output
  debugPrint("Averaged Current: "); 
  debugPrintln(currentAvg);
  debugPrint("Averaged Voltage: "); 
  debugPrintln(voltageAvg);
  debugPrint("Averaged Power: "); 
  debugPrintln(powerAvg);
}

void Read_PM25AQI()
{
  PM25_AQI_Data data;

  // Variables to accumulate values for averaging
  float pm10StandardSum = 0, pm25StandardSum = 0, pm100StandardSum = 0;
  float pm10EnvSum = 0, pm25EnvSum = 0, pm100EnvSum = 0;
  float particles03umSum = 0, particles05umSum = 0, particles10umSum = 0;
  float particles25umSum = 0, particles50umSum = 0, particles100umSum = 0;

  const int numReadings = 10;

  for (int i = 0; i < numReadings; i++) {
    if (aqi.read(&data)) {
      // Accumulate values
      pm10StandardSum += data.pm10_standard;
      pm25StandardSum += data.pm25_standard;
      pm100StandardSum += data.pm100_standard;

      pm10EnvSum += data.pm10_env;
      pm25EnvSum += data.pm25_env;
      pm100EnvSum += data.pm100_env;

      particles03umSum += data.particles_03um;
      particles05umSum += data.particles_05um;
      particles10umSum += data.particles_10um;
      particles25umSum += data.particles_25um;
      particles50umSum += data.particles_50um;
      particles100umSum += data.particles_100um;
    } else {
      debugPrintln("Failed to read from PM2.5 sensor!");
    }

    // Wait 500ms before the next reading
    delay(500);
  }

  // Calculate averages
  float pm10StandardAvg = pm10StandardSum / numReadings;
  float pm25StandardAvg = pm25StandardSum / numReadings;
  float pm100StandardAvg = pm100StandardSum / numReadings;

  float pm10EnvAvg = pm10EnvSum / numReadings;
  float pm25EnvAvg = pm25EnvSum / numReadings;
  float pm100EnvAvg = pm100EnvSum / numReadings;

  float particles03umAvg = particles03umSum / numReadings;
  float particles05umAvg = particles05umSum / numReadings;
  float particles10umAvg = particles10umSum / numReadings;
  float particles25umAvg = particles25umSum / numReadings;
  float particles50umAvg = particles50umSum / numReadings;
  float particles100umAvg = particles100umSum / numReadings;

  // Store averages into variables
  pm10_standard = pm10StandardAvg;
  pm25_standard = pm25StandardAvg;
  pm100_standard = pm100StandardAvg;

  pm10_env = pm10EnvAvg;
  pm25_env = pm25EnvAvg;
  pm100_env = pm100EnvAvg;

  // Convert float averages to uint16_t with rounding
  particles_03um = (uint16_t)round(particles03umAvg);
  particles_05um = (uint16_t)round(particles05umAvg);
  particles_10um = (uint16_t)round(particles10umAvg);
  particles_25um = (uint16_t)round(particles25umAvg);
  particles_50um = (uint16_t)round(particles50umAvg);
  particles_100um = (uint16_t)round(particles100umAvg);

  // Debug output
  debugPrint("Averaged PM10 (standard): "); debugPrintln(pm10_standard);
  debugPrint("Averaged PM2.5 (standard): "); debugPrintln(pm25_standard);
  debugPrint("Averaged PM100 (standard): "); debugPrintln(pm100_standard);

  debugPrint("Averaged PM10 (environmental): "); debugPrintln(pm10_env);
  debugPrint("Averaged PM2.5 (environmental): "); debugPrintln(pm25_env);
  debugPrint("Averaged PM100 (environmental): "); debugPrintln(pm100_env);

  debugPrint("Averaged Particles > 0.3um: "); debugPrintln(particles_03um);
  debugPrint("Averaged Particles > 0.5um: "); debugPrintln(particles_05um);
  debugPrint("Averaged Particles > 1.0um: "); debugPrintln(particles_10um);
  debugPrint("Averaged Particles > 2.5um: "); debugPrintln(particles_25um);
  debugPrint("Averaged Particles > 5.0um: "); debugPrintln(particles_50um);
  debugPrint("Averaged Particles > 50um: "); debugPrintln(particles_100um);
}

void Set_Time_Location(J *rsp)
{
  // Parse and set the time and location from the Notecard response
  time_t rawtime;
  rawtime = JGetNumber(rsp, "time");  

  lon = JGetNumber(rsp, "lon"); 
  lat = JGetNumber(rsp, "lat");

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

template <typename T>
void debugPrint(T message) {
#if DEBUG
  Serial.print(message);
#endif
}

template <typename T>
void debugPrintln(T message) {
#if DEBUG
  Serial.println(message);
#endif
}

