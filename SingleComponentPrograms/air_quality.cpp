#include <Arduino.h>
#include <Notecard.h>
#include <Adafruit_PM25AQI.h> // Air Quality Sensor
#include <Adafruit_INA260.h> // Voltage, Current, Power Sensor
#include <Adafruit_AHTX0.h> // Air Temperature and Humidity Sensor

// Object declaration for the PM2.5 AQI sensor
Adafruit_PM25AQI aqi;

// Variables to store particulate matter concentrations
float pm10_standard;
float pm25_standard;
float pm100_standard;
float pm10_env;
float pm25_env;
float pm100_env;

// Variables to store particle counts
uint16_t particles_03um;
uint16_t particles_05um;
uint16_t particles_10um;
uint16_t particles_25um;
uint16_t particles_50um;
uint16_t particles_100um;

// Function prototypes
void Read_PM25AQI();
void Print_Data();

void setup()
{
  delay(2000);  // Initial delay to allow peripherals to stabilize
  Serial.begin(115200);  // Initialize serial for debugging

  // Initialize the PM2.5 AQI sensor
  if (!aqi.begin_I2C()) {
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
  if (aqi.read(&data)) {
    pm10_standard = data.pm10_standard;  // Store PM1.0 concentration (standard)
    pm25_standard = data.pm25_standard;  // Store PM2.5 concentration (standard)
    pm100_standard = data.pm100_standard;  // Store PM10 concentration (standard)
    
    pm10_env = data.pm10_env;  // Store PM1.0 concentration (environmental)
    pm25_env = data.pm25_env;  // Store PM2.5 concentration (environmental)
    pm100_env = data.pm100_env;  // Store PM10 concentration (environmental)
    
    particles_03um = data.particles_03um;  // Particles > 0.3um / 0.1L air
    particles_05um = data.particles_05um;  // Particles > 0.5um / 0.1L air
    particles_10um = data.particles_10um;  // Particles > 1.0um / 0.1L air
    particles_25um = data.particles_25um;  // Particles > 2.5um / 0.1L air
    particles_50um = data.particles_50um;  // Particles > 5.0um / 0.1L air
    particles_100um = data.particles_100um;  // Particles > 50um / 0.1L air
  }
}

void Print_Data()
{
  // Print the particulate matter concentrations to the Serial monitor
  Serial.print("PM1.0 (standard): ");
  Serial.print(pm10_standard);
  Serial.println(" µg/m³");

  Serial.print("PM2.5 (standard): ");
  Serial.print(pm25_standard);
  Serial.println(" µg/m³");

  Serial.print("PM10 (standard): ");
  Serial.print(pm100_standard);
  Serial.println(" µg/m³");

  Serial.print("PM1.0 (environmental): ");
  Serial.print(pm10_env);
  Serial.println(" µg/m³");

  Serial.print("PM2.5 (environmental): ");
  Serial.print(pm25_env);
  Serial.println(" µg/m³");

  Serial.print("PM10 (environmental): ");
  Serial.print(pm100_env);
  Serial.println(" µg/m³");

  // Print particle counts for different sizes
  Serial.print("Particles > 0.3um / 0.1L air: ");
  Serial.println(particles_03um);

  Serial.print("Particles > 0.5um / 0.1L air: ");
  Serial.println(particles_05um);

  Serial.print("Particles > 1.0um / 0.1L air: ");
  Serial.println(particles_10um);

  Serial.print("Particles > 2.5um / 0.1L air: ");
  Serial.println(particles_25um);

  Serial.print("Particles > 5.0um / 0.1L air: ");
  Serial.println(particles_50um);

  Serial.print("Particles > 50um / 0.1L air: ");
  Serial.println(particles_100um);

  Serial.println();
}