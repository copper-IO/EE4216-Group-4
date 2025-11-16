/*
 * Camera Client Module
 * 
 * Manages communication with ESP32-CAM for photo capture.
 * Supports two modes:
 * 1. Real mode: Interfaces with actual ESP32-CAM over HTTP
 * 2. Mock mode: Returns random images from Lorem Picsum for testing
 * 
 * Real mode provides camera URL for downstream fetch/upload.
 * Connection check validates TCP connectivity and HTTP response.
 */

#include "camera_client.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClient.h>

static bool mockMode = false; // Start in real mode by default
static int mockCaptureCount = 0; // Counter for unique mock URLs

/*
 * Enable or disable mock mode
 * Mock mode returns placeholder images from web service
 * Real mode interfaces with actual ESP32-CAM hardware
 */
void CameraClient::setMockMode(bool enabled) {
  mockMode = enabled;
  Serial.println(mockMode ? "Camera: MOCK mode enabled" : "Camera: REAL mode enabled");
}

/*
 * Check current operating mode
 * Returns true if in mock mode, false if in real mode
 */
bool CameraClient::isMockMode() {
  return mockMode;
}

/*
 * Test camera connectivity and HTTP responsiveness
 * 
 * Process:
 * 1. Establish TCP connection to camera IP on port 80
 * 2. If connected, send HTTP GET request to root path
 * 3. Validate HTTP server responds (any positive code)
 * 
 * Returns true if camera is reachable and HTTP functional
 * Returns false if TCP fails or HTTP unresponsive
 */
bool CameraClient::checkConnection() {
  Serial.println("\n=== CAMERA CONNECTION TEST ===");
  Serial.println("[CAMERA] Testing connection to: " + String(CAM_IP));
  Serial.println("[CAMERA] Port: 80");
  
  WiFiClient testClient;
  testClient.setTimeout(5000); // 5-second TCP connection timeout
  
  Serial.print("[CAMERA] Attempting to connect");
  
  // Step 1: Test TCP connection to camera
  bool connected = testClient.connect(CAM_IP, 80);
  
  if (connected) {
    Serial.println(" ✓");
    Serial.println("[CAMERA] ✓✓✓ Camera is ONLINE and reachable! ✓✓✓");
    testClient.stop();
    
    // Step 2: Verify HTTP server is responding
    Serial.println("[CAMERA] Testing HTTP response...");
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(5000); // 5-second HTTP timeout
    http.begin(client, "http://" + String(CAM_IP) + "/");
    int httpCode = http.GET();
    http.end();
    
    if (httpCode > 0) {
      // HTTP server responded (any positive code is acceptable)
      Serial.println("[CAMERA] ✓ HTTP server responding (code: " + String(httpCode) + ")");
      Serial.println("[CAMERA] Camera web server is functional!");
      return true;
    } else {
      // TCP connected but HTTP not responding
      Serial.println("[CAMERA] ⚠ Connected but HTTP not responding");
      Serial.println("[CAMERA] Camera may be booting up...");
      return false;
    }
  } else {
    // TCP connection failed
    Serial.println(" ✗");
    Serial.println("[CAMERA] ✗✗✗ Camera is OFFLINE or unreachable! ✗✗✗");
    Serial.println("[CAMERA] Troubleshooting steps:");
    Serial.println("[CAMERA]   1. Check camera power supply");
    Serial.println("[CAMERA]   2. Verify camera is on same WiFi network: " + String(WIFI_SSID));
    Serial.println("[CAMERA]   3. Confirm camera IP is: " + String(CAM_IP));
    Serial.println("[CAMERA]   4. Check if camera WiFi LED is blinking/solid");
    Serial.println("[CAMERA]   5. Try pinging camera from computer: ping " + String(CAM_IP));
    return false;
  }
}

/*
 * Generate mock camera capture for testing
 * Returns JSON with URL to random placeholder image
 * Each call increments counter for unique image URLs
 */
String CameraClient::captureMock() {
  mockCaptureCount++;
  Serial.println("\n=== CAMERA MOCK CAPTURE ===");
  Serial.println("[MOCK] Capture count: #" + String(mockCaptureCount));
  
  // Generate URL to Lorem Picsum random image service
  // Query parameter ensures unique image each time
  String mockUrl = "https://picsum.photos/640/480?random=" + String(mockCaptureCount);
  Serial.println("[MOCK] Generated URL: " + mockUrl);
  Serial.println("[MOCK] Returning JSON response");
  
  // Return in same JSON format as real camera would
  return "{\"url\":\"" + mockUrl + "\"}";
}

/*
 * Capture photo from camera
 * 
 * Mock mode: Returns JSON with Lorem Picsum placeholder URL
 * Real mode: Returns direct HTTP URL to camera's /jpg endpoint
 * 
 * Real mode URL is used by Telegram module to fetch image bytes
 * for multipart upload (required for private IP cameras)
 */
String CameraClient::capture() {
  unsigned long startTime = millis();
  
  if (mockMode) {
    // Mock mode: Generate placeholder image URL
    String result = captureMock();
    unsigned long elapsed = millis() - startTime;
    Serial.println("[PERF] Mock camera capture took: " + String(elapsed) + "ms");
    return result;
  }

  // Real mode: Build camera JPEG endpoint URL
  // URL format: http://CAM_IP/jpg (e.g., http://192.168.1.100/jpg)
  String url = "http://" + String(CAM_IP) + "/jpg";
  Serial.println("[CAMERA] Providing camera URL: " + url);
  
  unsigned long elapsed = millis() - startTime;
  Serial.println("[PERF] Built camera URL in: " + String(elapsed) + "ms");
  
  return url; // Return URL for downstream fetch/upload
}