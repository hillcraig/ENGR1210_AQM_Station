#include <Arduino.h>
#include <Notecard.h>
#include <Adafruit_PM25AQI.h>   // Air Quality Sensor
#include <Adafruit_INA260.h>   // Voltage, Current, Power Sensor
#include <Adafruit_AHTX0.h>   // Air Temperature and Humidity Sensor
#include <SparkFun_I2C_Mux_Arduino_Library.h>

#define productUID "edu.umn.d.cshill:engr_1210_fall_2024"  // Product UID for Notecard

#define NOTECARD 1
#define AHTX0 1
#define PM25AQI 1
#define INA260 1

#define INA260_MUX_PORT 0
#define AHTX0_MUX_PORT 1
#define PM25AQI_MUX_PORT 2

#if NOTECARD
Notecard notecard;
void read_Notecard();
#endif
#if AHTX0
Adafruit_AHTX0 aht;
void read_AHTX0();
#endif
#if PM25AQI
Adafruit_PM25AQI aqi;
void read_PM25AQI();
#endif
#if INA260
Adafruit_INA260 ina260;
void read_INA260();
#endif
QWIICMUX myMux;

void setup() {
  // Wait for serial monitor to open
  delay(2000);
  Serial.begin(115200);

  Wire.begin();

  if (myMux.begin() == false)
  {
    Serial.println("Mux not detected. Freezing...");
    while (1);
  }
  Serial.println("Mux detected");

#if AHTX0
  myMux.setPort(AHTX0_MUX_PORT);
  if (!aht.begin()) {
    Serial.println("Could not find AHTX0 sensor!");
    while (1);  // Halt if sensor not found
  }
  Serial.println("AHTX0 sensor initialized!");
#endif

#if PM25AQI
  myMux.setPort(PM25AQI_MUX_PORT);
  if (!aqi.begin_I2C()) {
    Serial.println("Could not find PM2.5 sensor!");
    while (1);  // Halt if sensor not found
  }
  Serial.println("PM2.5 sensor initialized!");
#endif

#if INA260
  myMux.setPort(INA260_MUX_PORT);
  if (!ina260.begin()) {
    Serial.println("Couldn't find INA260 sensor!");
    while (1);  // Halt if sensor not found
  }
  Serial.println("INA260 sensor initialized!");

  // Set to average over 16 samples
  ina260.setAveragingCount(INA260_COUNT_16); 
#endif

#if NOTECARD
  notecard.begin(Serial1);
  notecard.setDebugOutputStream(Serial);

  // Configure Notecard settings
  {
    J *req = notecard.newRequest("hub.set");
    JAddStringToObject(req, "product", productUID);
    JAddStringToObject(req, "mode", "periodic");
    JAddNumberToObject(req, "inbound", 60 * 12);
    JAddNumberToObject(req, "outbound", 30);
    if (!notecard.sendRequest(req)) {
      JDelete(req);
    }
  }

  // Set AUX1 mode for DFU
  {
    J *req = notecard.newRequest("card.aux");
    JAddStringToObject(req, "mode", "dfu");
    if (!notecard.sendRequest(req)) {
      JDelete(req);
    }
  }

  // Set location mode to periodic updates
  {
    J *req = notecard.newRequest("card.location.mode");
    JAddStringToObject(req, "mode", "periodic");
    JAddNumberToObject(req, "seconds", 60 * 5);
    if (!notecard.sendRequest(req)) {
      JDelete(req);
    }
  }
#endif

  Serial.println("Setup complete\n");
}

void loop() {
#if NOTECARD
  read_Notecard();  // Read latitude and longitude from Notecard
#endif
#if PM25AQI
  myMux.setPort(PM25AQI_MUX_PORT);
  read_PM25AQI();   // Read air quality data
#endif
#if INA260
  myMux.setPort(INA260_MUX_PORT);
  read_INA260();    // Read current, voltage, and power data
#endif
#if AHTX0
  myMux.setPort(AHTX0_MUX_PORT);
  read_AHTX0();     // Read humidity and temperature data
#endif
  delay(3000);  // Wait before next loop iteration
}

#if AHTX0
void read_AHTX0() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);  // Get new data from AHTX0 sensor

  Serial.print("Temperature: ");
  Serial.print(temp.temperature);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(humidity.relative_humidity);
  Serial.println("% rH");
}
#endif

#if PM25AQI
void read_PM25AQI() {
  PM25_AQI_Data data;

  if (!aqi.read(&data)) {
    Serial.println("Could not read from AQI sensor");
    delay(500);  // Try again after a short delay
    return;
  }

  Serial.println("AQI reading success");
  Serial.println(F("---------------------------------------"));
  Serial.println(F("Concentration Units (standard)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: "));
  Serial.print(data.pm10_standard);
  Serial.print(F("\tPM 2.5: "));
  Serial.print(data.pm25_standard);
  Serial.print(F("\tPM 10: "));
  Serial.println(data.pm100_standard);

  Serial.println(F("Concentration Units (environmental)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: "));
  Serial.print(data.pm10_env);
  Serial.print(F("\tPM 2.5: "));
  Serial.print(data.pm25_env);
  Serial.print(F("\tPM 10: "));
  Serial.println(data.pm100_env);

  Serial.println(F("---------------------------------------"));
  Serial.print(F("Particles > 0.3µm / 0.1L air: "));
  Serial.println(data.particles_03um);
  Serial.print(F("Particles > 0.5µm / 0.1L air: "));
  Serial.println(data.particles_05um);
  Serial.print(F("Particles > 1.0µm / 0.1L air: "));
  Serial.println(data.particles_10um);
  Serial.print(F("Particles > 2.5µm / 0.1L air: "));
  Serial.println(data.particles_25um);
  Serial.print(F("Particles > 5.0µm / 0.1L air: "));
  Serial.println(data.particles_50um);
  Serial.print(F("Particles > 10µm / 0.1L air: "));
  Serial.println(data.particles_100um);
  Serial.println(F("---------------------------------------"));
}
#endif

#if INA260
void read_INA260() {
  Serial.print("Current: ");
  Serial.print(ina260.readCurrent());
  Serial.println(" mA");

  Serial.print("Bus Voltage: ");
  Serial.print(ina260.readBusVoltage());
  Serial.println(" mV");

  Serial.print("Power: ");
  Serial.print(ina260.readPower());
  Serial.println(" mW");

  Serial.println();
  delay(1000);  // Short delay between readings
}
#endif

#if NOTECARD
void read_Notecard() {
  size_t gps_time_s;
  double lat;
  double lon;

  // Get the last known GPS time
  {
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.location"));
    gps_time_s = JGetInt(rsp, "time");
    NoteDeleteResponse(rsp);
  }

  // Set location mode to continuous for immediate reading
  {
    J *req = notecard.newRequest("card.location.mode");
    JAddStringToObject(req, "mode", "continuous");
    notecard.sendRequest(req);
  }

  // Timeout after 10 minutes if location not acquired
  size_t timeout_s = 600;

  // Wait for GPS data to be available
  for (const size_t start_ms = ::millis();;) {
    if (::millis() >= (start_ms + (timeout_s * 1000))) {
      Serial.println("Timed out looking for a location\n");
      break;
    }

    // Check if new GPS data is available
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.location"));
    if (JGetInt(rsp, "time") != gps_time_s) {
      lat = JGetNumber(rsp, "lat");
      lon = JGetNumber(rsp, "lon");
      NoteDeleteResponse(rsp);

      // Restore previous location mode settings
      {
        J *req = notecard.newRequest("card.location.mode");
        JAddStringToObject(req, "mode", "periodic");
        notecard.sendRequest(req);
      }
      break;
    }

    // If Notecard cannot find GPS signal
    if (JGetObjectItem(rsp, "stop")) {
      Serial.println("Cannot find location\n");
      break;
    }

    NoteDeleteResponse(rsp);
    delay(2000);  // Wait before retrying
  }

  // Print the acquired GPS coordinates
  Serial.print("Module Latitude: ");
  Serial.println(lat);
  Serial.print("Module Longitude: ");
  Serial.println(lon);
}
#endif
