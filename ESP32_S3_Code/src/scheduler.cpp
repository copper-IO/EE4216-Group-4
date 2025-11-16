/*
 * Task Scheduler - FreeRTOS Task Management
 * 
 * This module creates and manages two concurrent tasks:
 * 1. SensorTask: Periodic environmental monitoring (every 30s)
 * 2. AlertTask: Motion detection and alert handling
 * 
 * Both tasks are pinned to Core 1, leaving Core 0 for main loop
 */

#include "scheduler.h"
#include "sensors.h"
#include "motion.h"
#include "camera_client.h"
#include "net_mqtt.h"
#include "alerts.h"
#include "config.h"

// Task handles for FreeRTOS task management
TaskHandle_t sensorTask, alertTask;

// Forward declarations of task functions
void taskSensor(void *pv);
void taskAlert(void *pv);

/*
 * Initialize and start all FreeRTOS tasks
 * Called once during setup() after WiFi and sensors are ready
 */
void Scheduler::initTasks() {
  Serial.println("\n=== INITIALIZING TASKS ===");
  
  // Initialize motion sensor BEFORE creating tasks to avoid race conditions
  // The interrupt must be configured before AlertTask starts polling
  Motion::init(PIRPIN);
  Serial.println("[SCHEDULER] Motion detection enabled on GPIO 15");
  delay(100); // Small delay after interrupt setup for stability
  
  // Create SensorTask on Core 1
  // Parameters: function, name, stack(8KB), params, priority(1), handle, core(1)
  Serial.println("[SCHEDULER] Creating SensorTask on Core 1...");
  xTaskCreatePinnedToCore(taskSensor, "SensorTask", 8192, NULL, 1, &sensorTask, 1);
  Serial.println("[SCHEDULER] ✓ SensorTask created");
  
  // Create AlertTask on Core 1
  Serial.println("[SCHEDULER] Creating AlertTask on Core 1...");
  xTaskCreatePinnedToCore(taskAlert,  "AlertTask",  8192, NULL, 1, &alertTask,  1);
  Serial.println("[SCHEDULER] ✓ AlertTask created");
  
  Serial.println("[SCHEDULER] All tasks initialized and running");
  Serial.println("=== SYSTEM READY ===");
  Serial.println();
}

/*
 * Sensor Task - Runs every 30 seconds
 * 
 * Responsibilities:
 * 1. Read temperature and humidity from DHT22
 * 2. Publish sensor data to Adafruit IO via MQTT
 * 3. Check for threshold violations and trigger weather alerts
 * 
 * Uses vTaskDelayUntil for fixed-period scheduling (no drift)
 */
void taskSensor(void *pv) {
  Serial.println("[TASK] SensorTask started");
  
  // Initialize timing for fixed-period scheduling
  TickType_t xLastWakeTime = xTaskGetTickCount();  // Current tick count
  const TickType_t xPeriod = pdMS_TO_TICKS(30000); // 30 second period
  
  for (;;) {  // Infinite loop - task never exits
    Serial.println("\n--- Sensor Task Cycle ---");
    
    // Step 1: Read temperature and humidity from DHT22
    // Returns struct with temp, humidity, and timestamp
    auto data = Sensors::readAll();
    
    // Step 2: Publish sensor data to Adafruit IO
    // Includes NaN validation and rate-limit spacing (1s between feeds)
    NetMQTT::publishEnv(data);
    
    // Step 3: Check for extreme weather conditions
    // Triggers alerts if temp > 34°C or humidity > 90%
    Alerts::checkWeatherAlerts(data);
    
    Serial.println("[TASK] Sensor task sleeping for 30 seconds...");
    
    // Sleep until next scheduled wake time
    // vTaskDelayUntil maintains exact 30s intervals regardless of work duration
    // xLastWakeTime is automatically advanced by xPeriod each cycle
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

/*
 * Alert Task - Monitors motion detection
 * 
 * Responsibilities:
 * 1. Poll motion detection flag every second
 * 2. When motion detected, capture photo from camera
 * 3. Send alert to Telegram (with photo) and MQTT
 * 4. Enforce 60-second cooldown between alerts
 * 
 * Runs continuously with 1-second polling interval
 */
void taskAlert(void *pv) {
  Serial.println("[TASK] AlertTask started - monitoring for motion");
  
  // Track last alert time for cooldown enforcement
  unsigned long lastAlertTime = 0;
  const unsigned long ALERT_COOLDOWN = 60000; // 60 seconds cooldown
  
  for (;;) {  // Infinite loop - task never exits
    // Check if motion was detected (flag set by ISR)
    if (Motion::motionDetected()) {
      unsigned long now = millis();
      
      // Only process alert if cooldown period has elapsed
      if (now - lastAlertTime >= ALERT_COOLDOWN) {
        // Step 1: Capture photo from camera (real or mock)
        // Returns URL or JSON with image location
        String photoURL = CameraClient::capture();
        
        // Step 2: Send alerts via multiple channels
        // Telegram receives photo + caption
        // MQTT receives text-only alert
        Alerts::handleMotionAlert(photoURL);
        
        // Update last alert time to start new cooldown period
        lastAlertTime = now;
        Serial.println("[ALERT] Cooldown active for 60 seconds");
      } else {
        // Motion detected but still in cooldown - ignore
        Serial.println("[ALERT] Motion detected but in cooldown period - ignoring");
      }
    }
    
    // Sleep for 1 second before next check
    // Polling-based approach (could be optimized with task notifications)
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
