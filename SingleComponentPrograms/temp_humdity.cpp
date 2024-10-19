#include <Arduino.h>
#include <Adafruit_AHTX0.h>

// Object declaration for the AHTX0 sensor
Adafruit_AHTX0 aht;

// Variables to store temperature and humidity
float temperature;
float humidity;

void setup()
{
  delay(2000);  // Initial delay to allow peripherals to stabilize
  Serial.begin(115200);  // Initialize serial for debugging

  // Initialize the AHTX0 sensor (Temperature & Humidity)
  if (! aht.begin()) {
    Serial.println("Could not find AHTX0 sensor!");
    while (1);  // Stop the program if the sensor is not found
  }
  Serial.println("AHTX0 sensor found!");
}

void loop()
{
  Read_AHTX0();  // Read the sensor data
  Print_Data();  // Print the data to Serial

  delay(5000);  // Wait for 5 seconds before reading again
}

void Read_AHTX0()
{
  // Read temperature and humidity from AHTX0 sensor
  sensors_event_t humid, temp;
  aht.getEvent(&humid, &temp);
  temperature = temp.temperature;  // Store temperature value
  humidity = humid.relative_humidity;  // Store humidity value
}

void Print_Data()
{
  // Print the temperature and humidity to the Serial monitor
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.println();
}