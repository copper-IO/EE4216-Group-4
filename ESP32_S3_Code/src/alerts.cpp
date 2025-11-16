/*
 * Alerts Module - Coordinated Alert Handling
 * 
 * Manages two types of alerts:
 * 1. Weather alerts: Temperature and humidity threshold violations
 * 2. Motion alerts: Intrusion detection with photo evidence
 * 
 * Implements state machines to prevent alert spam and ensure
 * users receive one actionable notification per event.
 */

#include "alerts.h"
#include "config.h"
#include "telegram.h"
#include "net_mqtt.h"

// Static member initialization - tracks alert state
// These persist across function calls to implement "send once" behavior
bool Alerts::tempAlertSent = false;   // True if temp alert active
bool Alerts::humAlertSent = false;    // True if humidity alert active

/*
 * Check environmental sensor data for threshold violations
 * 
 * Implements a "send-on-crossing" state machine:
 * - Alert sent once when threshold is exceeded (rising edge)
 * - No repeated alerts while value remains high
 * - Alert state resets when value returns to normal
 * 
 * This prevents notification spam during prolonged violations.
 * 
 * @param data: SensorData struct with temp, humidity, timestamp
 */
void Alerts::checkWeatherAlerts(SensorData data) {
  // === Temperature Alert Check ===
  if (!isnan(data.temp) && data.temp > TEMP_LIMIT) {
    // Temperature exceeds configured limit
    if (!tempAlertSent) {
      // First crossing of threshold - send alert
      Serial.println("[ALERT] 🌡️  EXTREME TEMPERATURE DETECTED!");
      
      // Format alert message with current value and limit
      String msg = "⚠️ HIGH TEMPERATURE ALERT: " + String(data.temp, 1) + "°C (Limit: " + String(TEMP_LIMIT) + "°C)";
      
      // Send to Telegram for immediate user notification
      Telegram::sendAlert(msg, "");  // No photo for weather alerts
      
      // Rate limiting: wait 1s before MQTT publish to respect API limits
      vTaskDelay(pdMS_TO_TICKS(1000));
      
      // Publish to MQTT dashboard feed
      NetMQTT::publishAlert("high_temperature", "");
      
      // Mark alert as sent to prevent repeated notifications
      tempAlertSent = true;
    }
    // else: already sent alert, don't spam while temp remains high
  } else {
    // Temperature back to normal or reading invalid
    tempAlertSent = false; // Reset state for next threshold crossing
  }
  
  // === Humidity Alert Check ===
  // Same logic as temperature but for humidity threshold
  if (!isnan(data.hum) && data.hum > HUM_LIMIT) {
    // Humidity exceeds configured limit
    if (!humAlertSent) {
      // First crossing of threshold - send alert
      Serial.println("[ALERT] 💧 EXTREME HUMIDITY DETECTED!");
      
      // Format alert message
      String msg = "⚠️ HIGH HUMIDITY ALERT: " + String(data.hum, 1) + "% (Limit: " + String(HUM_LIMIT) + "%)";
      
      // Send to Telegram
      Telegram::sendAlert(msg, "");
      
      // Rate limiting delay
      vTaskDelay(pdMS_TO_TICKS(1000));
      
      // Publish to MQTT
      NetMQTT::publishAlert("high_humidity", "");
      
      // Mark as sent
      humAlertSent = true;
    }
  } else {
    // Humidity back to normal
    humAlertSent = false; // Reset state
  }
}

/*
 * Handle motion detection alert with photo evidence
 * 
 * Multi-step process:
 * 1. Send Telegram alert with photo and caption
 * 2. Publish text-only alert to MQTT dashboard
 * 3. Log performance metrics
 * 
 * Note: Photo is only sent to Telegram (not MQTT) due to:
 * - MQTT payload size limitations
 * - Adafruit IO transfer limits on free tier
 * - Better user experience viewing photos in Telegram
 * 
 * @param photoURL: Camera image URL or JSON with image location
 */
void Alerts::handleMotionAlert(String photoURL) {
  unsigned long startTime = millis();  // Track execution time
  
  Serial.println("\n\n🚨 ========== MOTION ALERT TRIGGERED ========== 🚨");
  
  // === Step 1: Send Telegram notification with photo ===
  Serial.println("[ALERT] Step 1: Sending Telegram notification with photo...");
  // Telegram receives rich alert: photo + "Motion detected" caption
  // Photo delivery handled by two-path system (URL or multipart upload)
  Telegram::sendAlert("Motion detected", photoURL);
  
  // === Step 2: Publish MQTT alert ===
  Serial.println("[ALERT] Step 2: Publishing simple alert to MQTT...");
  // Rate limiting: 1s delay before MQTT to avoid API throttling
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // MQTT gets text-only alert for dashboard notification
  // No photo URL - keeps payload small and respects Adafruit IO limits
  NetMQTT::publishAlert("motion", ""); // No photo URL - Telegram only
  
  // === Performance Logging ===
  unsigned long elapsed = millis() - startTime;
  Serial.println("[ALERT] ✓ Motion alert sequence completed");
  Serial.println("[PERF] Total motion alert took: " + String(elapsed) + "ms");
  Serial.println("🚨 ============================================== 🚨\n");
}
