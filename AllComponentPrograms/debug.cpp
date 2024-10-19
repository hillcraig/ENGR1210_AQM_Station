#include <Arduino.h>
#include <Notecard.h>
#include <Adafruit_PM25AQI.h> // Air Quality Sensor
#include <Adafruit_INA260.h> // Voltage, Current, Power Sensor
#include <Adafruit_AHTX0.h> // Air Temperature and Humidity Sensor

#define productUID "com.gmail.liamgaeuman:environmental_monitoring_buoy" 

#define NOTECARD 1
#define AHTX0 1
#define PM25AQI 1
#define INA260 1

void read_AHTX0();
void read_INA260();
void read_Notecard();
void read_PM25AQI();

#if NOTECARD
Notecard notecard;
#endif
#if AHTX0
Adafruit_AHTX0 aht;
#endif
#if PM25AQI
Adafruit_PM25AQI aqi;
#endif
#if INA260
Adafruit_INA260 ina260;
#endif

void setup() {
  // Wait for serial monitor to open
  delay(2000);
  Serial.begin(115200);
  
#if NOTECARD
  notecard.setDebugOutputStream(Serial);
#endif

#if AHTX0
  if (! aht.begin()) {
    Serial.println("Could not find AHTX0 sensor!");
    while (1);
  }
  Serial.println("AHTX0 found!");
#endif

#if PM25AQI
  if (! aqi.begin_I2C()) {     
    Serial.println("Could not find PM 2.5 sensor!");
    while (1);
  }
  Serial.println("PM25 found!");
#endif
#if INA260
  if (!ina260.begin()) {
    Serial.println("Couldn't find INA260 sensor!");
    while (1);
  }
  Serial.println("INA260 found!");
#endif
#if NOTECARD
  notecard.begin();

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

  {
    // Pull AUX1 low during DFU
    J *req = notecard.newRequest("card.aux");
    JAddStringToObject(req, "mode", "dfu");
    if (!notecard.sendRequest(req)) {
      JDelete(req);
    }
  }

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
  read_Notecard();  // read lat lon
#endif
#if PM25AQI
  read_PM25AQI();  // read air quality
#endif
#if INA260
  read_INA260();  // read current, voltage, and power
#endif
#if AHTX0
  read_AHTX0();  // read humidity and temp
#endif
  delay(2000);  // meow meow
}

#if AHTX0
void read_AHTX0()
{
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");
}
#endif

#if PM25AQI
void read_PM25AQI()
{
  //Air Quality Sensor
  PM25_AQI_Data data;
  
  if (! aqi.read(&data)) {
    Serial.println("Could not read from AQI");
    delay(500);  // try again in a bit!
    return;
  }

  Serial.println("AQI reading success");
  Serial.println(F("---------------------------------------"));
  Serial.println(F("Concentration Units (standard)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_standard);
  Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_standard);
  Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_standard);
  Serial.println(F("Concentration Units (environmental)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_env);
  Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_env);
  Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_env);
  Serial.println(F("---------------------------------------"));
  Serial.print(F("Particles > 0.3um / 0.1L air:")); Serial.println(data.particles_03um);
  Serial.print(F("Particles > 0.5um / 0.1L air:")); Serial.println(data.particles_05um);
  Serial.print(F("Particles > 1.0um / 0.1L air:")); Serial.println(data.particles_10um);
  Serial.print(F("Particles > 2.5um / 0.1L air:")); Serial.println(data.particles_25um);
  Serial.print(F("Particles > 5.0um / 0.1L air:")); Serial.println(data.particles_50um);
  Serial.print(F("Particles > 10 um / 0.1L air:")); Serial.println(data.particles_100um);
  Serial.println(F("---------------------------------------"));
}
#endif

#if INA260
void read_INA260()
{
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
  delay(1000);
}
#endif

#if NOTECARD
void read_Notecard()
{
  size_t gps_time_s;
  double lat;
  double lon;
  
  {
    // Save the time from the last location reading.
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.location"));
    gps_time_s = JGetInt(rsp, "time");
    NoteDeleteResponse(rsp);
  }

  {
    // Set the location mode to "continuous" mode to force the
    // Notecard to take an immediate GPS/GNSS reading.
    J *req = notecard.newRequest("card.location.mode");
    JAddStringToObject(req, "mode", "continuous");
    notecard.sendRequest(req);
  }

  // How many seconds to wait for a location before you stop looking
  size_t timeout_s = 600;

  // Block while resolving GPS/GNSS location
  for (const size_t start_ms = ::millis();;) {
    // Check for a timeout, and if enough time has passed, break out of the loop
    // to avoid looping forever
    if (::millis() >= (start_ms + (timeout_s * 1000))) {
      Serial.println("Timed out looking for a location\n");
      break;
    }
  
    // Check if GPS/GNSS has acquired location information
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.location"));
    if (JGetInt(rsp, "time") != gps_time_s) {
      lat = JGetNumber(rsp, "lat");
      lon = JGetNumber(rsp, "lon");
      NoteDeleteResponse(rsp);

      // Restore previous configuration
      {
        J *req = notecard.newRequest("card.location.mode");
        JAddStringToObject(req, "mode", "periodic");
        notecard.sendRequest(req);
      }
      break;
    }
  
    // If a "stop" field is on the card.location response, it means the Notecard
    // cannot locate a GPS/GNSS signal, so we break out of the loop to avoid looping
    // endlessly
    if (JGetObjectItem(rsp, "stop")) {
      Serial.println("Found a stop flag, cannot find location\n");
      break;
    }
  
    NoteDeleteResponse(rsp);
    // Wait 2 seconds before trying again
    delay(2000);
  }

  Serial.print("Module Latitude: ");
  Serial.println(lat);
  Serial.print("Module Longitude: ");
  Serial.println(lon);
}
#endif
