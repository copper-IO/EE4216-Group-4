/*
 * MQTT Network Module - Adafruit IO Cloud Integration
 * 
 * Handles all MQTT communication with Adafruit IO cloud service.
 * Publishes sensor data to three separate feeds:
 * - temperature (°C values)
 * - humidity (% values)
 * - alerts (text event messages)
 * 
 * Implements connection retry logic and rate limiting to comply
 * with Adafruit IO free tier restrictions (30 data points/minute).
 */

#include "net_mqtt.h"
#include "config.h"
#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

// Adafruit IO MQTT Setup
WiFiClient client;  // WiFi client for TCP connection
Adafruit_MQTT_Client mqtt(&client, "io.adafruit.com", 1883, IO_USERNAME, IO_KEY);

// Adafruit IO Feeds - one per data type
// Feed names must match your Adafruit IO dashboard configuration
Adafruit_MQTT_Publish tempFeed = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humFeed = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish alertFeed = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/alerts");

/*
 * Connect to MQTT broker with retry logic
 * 
 * Attempts connection up to 3 times with 5-second delays.
 * If connection fails, logs error but continues (graceful degradation).
 * System will retry on next publish attempt.
 */
void MQTT_connect() {
  if (mqtt.connected()) {
    Serial.println("[MQTT] Already connected");
    return;
  }
  
  Serial.println("\n=== MQTT CONNECTION ===");
  Serial.println("[MQTT] Connecting to io.adafruit.com:1883");
  Serial.println("[MQTT] Username: " + String(IO_USERNAME));
  int8_t ret;
  uint8_t retries = 3;
  
  while ((ret = mqtt.connect()) != 0) {
    Serial.print("[MQTT] Connection failed: ");
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("[MQTT] Retry attempt " + String(4 - retries) + "/3");
    Serial.println("[MQTT] Waiting 5 seconds before retry...");
    mqtt.disconnect();
    delay(5000);
    retries--;
    if (retries == 0) {
      Serial.println("[MQTT] ERROR: Connection failed after 3 attempts. Continuing anyway.");
      return;
    }
  }
  Serial.println("[MQTT] ✓ Connected successfully!");
  Serial.println("[MQTT] Ready to publish data");
}

/*
 * Initialize MQTT connection
 * Called once during setup()
 */
void NetMQTT::init() {
  MQTT_connect();
}

/*
 * Publish environmental sensor data to Adafruit IO
 * 
 * Publishes temperature and humidity to separate feeds with:
 * - NaN validation (skips invalid readings)
 * - 1-second delay between publishes (rate limit compliance)
 * - Performance logging
 * 
 * @param d: SensorData struct with temp, humidity, timestamp
 */
void NetMQTT::publishEnv(SensorData d) {
  unsigned long startTime = millis();
  Serial.println("\n=== PUBLISHING SENSOR DATA ===");
  MQTT_connect();  // Ensure connection is active
  
  // Publish temperature and humidity to separate feeds
  if (!isnan(d.temp)) {
    Serial.print("[MQTT] Publishing temperature: ");
    Serial.print(d.temp);
    Serial.println("°C");
    if (tempFeed.publish(d.temp)) {
      Serial.println("[MQTT] ✓ Temperature published successfully");
    } else {
      Serial.println("[MQTT] ✗ Failed to publish temperature");
    }
    delay(1000); // 1 second delay between publishes (rate limit: 2 per second)
  } else {
    Serial.println("[MQTT] ⚠ Temperature reading is NaN, skipping");
  }
  
  if (!isnan(d.hum)) {
    Serial.print("[MQTT] Publishing humidity: ");
    Serial.print(d.hum);
    Serial.println("%");
    if (humFeed.publish(d.hum)) {
      Serial.println("[MQTT] ✓ Humidity published successfully");
    } else {
      Serial.println("[MQTT] ✗ Failed to publish humidity");
    }
  } else {
    Serial.println("[MQTT] ⚠ Humidity reading is NaN, skipping");
  }
  
  unsigned long elapsed = millis() - startTime;
  Serial.println("[PERF] MQTT publish took: " + String(elapsed) + "ms");
}

/*
 * Publish alert event to Adafruit IO alerts feed
 * 
 * Formats and publishes alert messages to dashboard.
 * Photo URLs are not included (Telegram-only for images).
 * 
 * @param reason: Alert type (e.g., "motion", "high_temperature")
 * @param photoURL: Image URL (not used for MQTT, kept for API consistency)
 */
void NetMQTT::publishAlert(String reason, String photoURL) {
  Serial.println("\n=== PUBLISHING ALERT ===");
  Serial.println("[MQTT] Alert reason: " + reason);
  MQTT_connect();
  
  // Send alert as formatted string
  String alertMsg = reason;
  if (photoURL.length() > 0) {
    Serial.println("[MQTT] Photo URL: " + photoURL);
    alertMsg += " | Photo: " + photoURL;
  } else {
    Serial.println("[MQTT] No photo URL provided");
  }
  
  Serial.println("[MQTT] Publishing to alerts feed...");
  if (alertFeed.publish(alertMsg.c_str())) {
    Serial.println("[MQTT] ✓ Alert published successfully");
    Serial.println("[MQTT] Message: " + alertMsg);
  } else {
    Serial.println("[MQTT] ✗ Failed to publish alert");
  }
}