#include <Arduino.h>
#include <Notecard.h>
#include <Adafruit_PM25AQI.h> // Air Quality Sensor
#include <Adafruit_INA260.h> // Voltage, Current, Power Sensor
#include <Adafruit_AHTX0.h> // Air Temperature and Humidity Sensor

// Object declaration for the PM2.5 AQI sensor
Adafruit_PM25AQI aqi;

// Variables to store particulate matter concentrations
float pm10;
float pm25;
float pm100;

// Function prototypes
void Read_PM25AQI();
void Print_Data();

void setup()
{
  delay(2000);  // Initial delay to allow peripherals to stabilize
  Serial.begin(115200);  // Initialize serial for debugging

  // Initialize the PM2.5 AQI sensor
  if (! aqi.begin_I2C()) {
    Serial.println("Could not find PM 2.5 sensor!");
    while (1);  // Stop the program if the sensor is not found
  }
  Serial.println("PM2.5 sensor found!");
}

void loop()
{
  Read_PM25AQI();  // Read the sensor data
  Print_Data();  // Print the data to Serial

  delay(5000);  // Wait for 5 seconds before reading again
}

void Read_PM25AQI()
{
  PM25_AQI_Data data;
  
  // Read air quality data from PM2.5 AQI sensor
  aqi.read(&data);

  pm10 = data.pm10_standard;  // Store PM10 concentration
  pm25 = data.pm25_standard;  // Store PM2.5 concentration
  pm100 = data.pm100_standard;  // Store PM100 concentration
}

void Print_Data()
{
  // Print the particulate matter concentrations to the Serial monitor
  Serial.print("PM1.0: ");
  Serial.print(pm10);
  Serial.println(" µg/m³");

  Serial.print("PM2.5: ");
  Serial.print(pm25);
  Serial.println(" µg/m³");

  Serial.print("PM10: ");
  Serial.print(pm100);
  Serial.println(" µg/m³");

  Serial.println();
}