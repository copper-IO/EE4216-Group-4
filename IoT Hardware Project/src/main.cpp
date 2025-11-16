/*
 * IoT Smart Home Security System - Main Entry Point
 * 
 * This file handles system initialization and main loop execution.
 * Key responsibilities:
 * - WiFi connection with retry and restart logic
 * - Sensor, MQTT, and camera initialization
 * - FreeRTOS task creation for concurrent operation
 * - Periodic health monitoring
 */

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "scheduler.h"
#include "net_mqtt.h"
#include "telegram.h"
#include "sensors.h"
#include "motion.h"
#include "camera_client.h"

void setup() {
  // Initialize serial communication at 115200 baud for debugging
  Serial.begin(115200);
  delay(1000);  // Allow serial to stabilize
  
  // Display startup banner with system information
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════╗");
  Serial.println("║   IoT Smart Home Security System v1.0     ║");
  Serial.println("║   ESP32-S3 DevKit                         ║");
  Serial.println("╚════════════════════════════════════════════╝");
  Serial.println();
  Serial.println("[BOOT] Starting initialization sequence...");
  Serial.println("[BOOT] ESP32-S3 @ 240MHz");
  Serial.println("[BOOT] Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println();

  // === WiFi Connection Setup ===
  Serial.println("=== WIFI CONNECTION ===");
  Serial.println("[WIFI] Mode: Station (STA)");
  Serial.println("[WIFI] SSID: " + String(WIFI_SSID));
  
  // Configure ESP32 as WiFi client (not access point)
  WiFi.mode(WIFI_STA);
  
  // Attempt to connect with credentials from config.h
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  // Connection retry loop with timeout protection
  Serial.print("[WIFI] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Restart if connection fails after 60 attempts (30 seconds)
    if (attempts > 60) {
      Serial.println("\n[WIFI] ✗ Connection timeout!");
      Serial.println("[WIFI] Restarting...");
      ESP.restart();  // Hard restart to retry from clean state
    }
  }
  
  // Connection successful - display network info
  Serial.println();
  Serial.println("[WIFI] ✓ Connected!");
  Serial.println("[WIFI] IP Address: " + WiFi.localIP().toString());
  Serial.println("[WIFI] Signal Strength: " + String(WiFi.RSSI()) + " dBm");
  Serial.println();

  // === Sensor Initialization ===
  // Initialize DHT22 temperature/humidity sensor with stabilization delay
  Sensors::init();
  
  // === MQTT Client Initialization ===
  // Connect to Adafruit IO for cloud data publishing
  NetMQTT::init();
  
  // === Camera Initialization and Connection Check ===
  Serial.println("=== CAMERA INITIALIZATION ===");
  Serial.println("[CAMERA] Mode: " + String(CameraClient::isMockMode() ? "MOCK" : "REAL"));
  Serial.println("[CAMERA] Target IP: " + String(CAM_IP));
  
  // Only check connection if not in mock mode
  if (!CameraClient::isMockMode()) {
    bool cameraOnline = CameraClient::checkConnection();
    if (!cameraOnline) {
      // Camera unavailable but system continues with degraded functionality
      Serial.println("[CAMERA] ⚠ WARNING: Camera not detected!");
      Serial.println("[CAMERA] System will continue but camera features unavailable");
      Serial.println("[CAMERA] To use mock camera instead, call CameraClient::setMockMode(true)");
    }
  } else {
    Serial.println("[CAMERA] Mock mode active - using placeholder images");
  }
  
  // === FreeRTOS Task Initialization ===
  // Create and start sensor monitoring and alert handling tasks
  Scheduler::initTasks();
  
  Serial.println("\n✓✓✓ SYSTEM FULLY OPERATIONAL ✓✓✓\n");
}

void loop() {
  // Main loop runs on Core 0 - tasks run on Core 1
  // Keep this minimal to avoid blocking the scheduler
  vTaskDelay(pdMS_TO_TICKS(10000));  // Sleep for 10 seconds
  
  // Periodic health check every 10 seconds
  static unsigned long lastHealthCheck = 0;
  if (millis() - lastHealthCheck > 10000) {
    // Log system uptime and available heap memory
    Serial.println("[HEALTH] System uptime: " + String(millis() / 1000) + "s | Free heap: " + String(ESP.getFreeHeap()) + " bytes");
    lastHealthCheck = millis();
  }
}
