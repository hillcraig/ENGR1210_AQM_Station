#include <Arduino.h>
#include <Notecard.h>
#include <Adafruit_PM25AQI.h> // Air Quality Sensor
#include <Adafruit_INA260.h> // Voltage, Current, Power Sensor
#include <Adafruit_AHTX0.h> // Air Temperature and Humidity Sensor

// Object declaration for the INA260 sensor
Adafruit_INA260 ina260;

// Variables to store current, voltage, and power
float current;
float voltage;
float power;

// Function prototypes
void Print_Data();
void Read_INA260();

void setup()
{
  delay(2000);  // Initial delay to allow peripherals to stabilize
  Serial.begin(115200);  // Initialize serial for debugging

  // Initialize the INA260 sensor (Power, Current, Voltage)
  if (!ina260.begin()) {
    Serial.println("Could not find INA260 sensor!");
    while (1);  // Stop the program if the sensor is not found
  }
  Serial.println("INA260 sensor found!");

  // Set to average over 16 samples
  ina260.setAveragingCount(INA260_COUNT_16);
}

void loop()
{
  Read_INA260();  // Read the sensor data
  Print_Data();  // Print the data to Serial

  delay(5000);  // Wait for 5 seconds before reading again
}

void Read_INA260()
{
  // Read current, voltage, and power from INA260 sensor
  current = ina260.readCurrent();  // Store current value
  voltage = ina260.readBusVoltage();  // Store voltage value
  power = ina260.readPower();  // Store power value
}

void Print_Data()
{
  // Print the current, voltage, and power to the Serial monitor
  Serial.print("Current: ");
  Serial.print(current);
  Serial.println(" mA");

  Serial.print("Bus Voltage: ");
  Serial.print(voltage);
  Serial.println(" mV");

  Serial.print("Power: ");
  Serial.print(power);
  Serial.println(" mW");

  Serial.println();
}