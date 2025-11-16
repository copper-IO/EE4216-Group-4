/*
 * Sensors Module - DHT22 Temperature and Humidity Sensor Interface
 * 
 * Handles initialization and reading of the DHT22 digital sensor.
 * Provides data validation, timestamp generation, and error handling.
 */

#include "sensors.h"
#include <DHT.h>
#include "config.h"
#include "utils.h"

// Create DHT sensor object with pin and type from config.h
static DHT dht(DHTPIN, DHTTYPE);

/*
 * Initialize DHT22 sensor
 * 
 * The DHT22 requires a stabilization period after power-on.
 * This function waits 5 seconds before the sensor is ready for readings.
 */
void Sensors::init() {
  Serial.println("\n=== DHT SENSOR INIT ===");
  Serial.println("[DHT] Type: DHT22");
  Serial.println("[DHT] Pin: " + String(DHTPIN));
  
  // Initialize the DHT library and configure the pin
  dht.begin();
  Serial.println("[DHT] Sensor initialized");
  
  Serial.println("[DHT] Waiting for sensor to stabilize...");
  // DHT22 needs 5 seconds to stabilize after power-on
  // Without this delay, first readings may be NaN or inaccurate
  delay(5000);
  Serial.println("[DHT] Sensor ready");
}

/*
 * Read all sensor values (temperature, humidity, timestamp)
 * 
 * Returns a SensorData struct containing:
 * - temp: Temperature in Celsius (float, may be NaN on error)
 * - hum: Humidity in percent (float, may be NaN on error)
 * - ts: ISO 8601 timestamp string
 * 
 * Validates readings and logs warnings if thresholds are exceeded.
 */
SensorData Sensors::readAll() {
  unsigned long startTime = millis();  // Track execution time
  Serial.println("\n=== READING SENSORS ===");
  SensorData s;  // Create struct to hold sensor data
  
  // === Read Temperature ===
  Serial.println("[DHT] Reading temperature...");
  s.temp = dht.readTemperature();  // Returns Celsius, or NaN on failure
  
  // Validate temperature reading
  if (isnan(s.temp)) {
    // Sensor communication failed or sensor not ready
    Serial.println("[DHT] ✗ Temperature read failed (NaN)");
  } else {
    // Valid reading - display and check against threshold
    Serial.print("[DHT] ✓ Temperature: ");
    Serial.print(s.temp);
    Serial.println("°C");
    
    // Warn if temperature exceeds configured limit
    if (s.temp > TEMP_LIMIT) {
      Serial.println("[DHT] ⚠️  WARNING: Temperature above limit (" + String(TEMP_LIMIT) + "°C)");
    }
  }
  
  // === Read Humidity ===
  Serial.println("[DHT] Reading humidity...");
  s.hum = dht.readHumidity();  // Returns percentage, or NaN on failure
  
  // Validate humidity reading
  if (isnan(s.hum)) {
    // Sensor communication failed or sensor not ready
    Serial.println("[DHT] ✗ Humidity read failed (NaN)");
  } else {
    // Valid reading - display and check against threshold
    Serial.print("[DHT] ✓ Humidity: ");
    Serial.print(s.hum);
    Serial.println("%");
    
    // Warn if humidity exceeds configured limit
    if (s.hum > HUM_LIMIT) {
      Serial.println("[DHT] ⚠️  WARNING: Humidity above limit (" + String(HUM_LIMIT) + "%)");
    }
  }
  
  // === Add Timestamp ===
  // Generate ISO 8601 format timestamp for data logging
  s.ts = Utils::timestamp();
  Serial.println("[DHT] Timestamp: " + s.ts);
  
  // === Performance Logging ===
  // Measure and log how long the sensor read took
  unsigned long elapsed = millis() - startTime;
  Serial.println("[PERF] Sensor reading took: " + String(elapsed) + "ms");
  
  return s;  // Return populated struct (may contain NaN values)
}