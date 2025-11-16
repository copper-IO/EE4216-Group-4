/*
 * Motion Detection Module - PIR Sensor Interface
 * 
 * Implements interrupt-driven motion detection with debouncing.
 * Uses a volatile flag set by ISR and checked by AlertTask.
 */

#include "motion.h"

// Volatile variables for ISR communication
// Must be volatile because they're modified in ISR and read in main code
volatile bool motionFlag = false;  // Set to true when motion detected
volatile unsigned long lastTriggerTime = 0;  // Timestamp of last trigger for debouncing

/*
 * Interrupt Service Routine (ISR) for motion detection
 * 
 * IRAM_ATTR: Stores function in IRAM for fast execution
 * Called automatically when PIR sensor triggers RISING edge
 * 
 * Implements 5-second debounce to prevent false triggers from:
 * - PIR sensor oscillation after detection
 * - Multiple rapid movements
 * - Electrical noise
 */
void IRAM_ATTR onMotion() { 
  unsigned long now = millis();  // Current time in milliseconds
  
  // Debounce logic: only set flag if >5 seconds since last trigger
  if (now - lastTriggerTime > 5000) {
    motionFlag = true;  // Signal to AlertTask that motion was detected
    lastTriggerTime = now;  // Record trigger time for next debounce check
  }
  // If < 5 seconds, ignore this trigger (debounce suppression)
}

/*
 * Initialize motion sensor with interrupt handling
 * 
 * @param pin: GPIO pin number for PIR sensor output
 * 
 * Configuration steps:
 * 1. Enable internal pull-down resistor for stable LOW state
 * 2. Attach interrupt on RISING edge (LOW->HIGH transition)
 * 3. ISR debounces triggers to prevent false positives
 */
void Motion::init(int pin) {
  Serial.println("\n=== MOTION SENSOR INIT ===");
  Serial.println("[MOTION] Configuring PIR on pin: " + String(pin));
  
  // Configure pin with internal pull-down resistor
  // Pull-down ensures pin reads LOW when PIR is idle
  // Without pull-down, floating input can cause false triggers
  pinMode(pin, INPUT_PULLDOWN);
  
  delay(50); // Let the pull-down circuitry stabilize
  
  // Attach interrupt handler to this pin
  // Triggers on RISING edge (LOW->HIGH) when PIR detects motion
  // onMotion() ISR will be called automatically by hardware
  attachInterrupt(pin, onMotion, RISING);
  
  Serial.println("[MOTION] Interrupt attached on RISING edge with pull-down");
  Serial.println("[MOTION] Debounce time: 5 seconds");
  Serial.println("[MOTION] Sensor ready");
}

/*
 * Check if motion was detected since last check
 * 
 * Returns true if motion flag was set by ISR, false otherwise.
 * Automatically clears the flag after returning true (single-shot behavior).
 * 
 * Called by AlertTask every second to poll for motion events.
 */
bool Motion::motionDetected() {
  if (motionFlag) {
    Serial.println("\n⚠️  [MOTION] DETECTED! Flag was set by interrupt");
    motionFlag = false;  // Clear flag so we don't re-process this event
    return true;
  }
  return false;  // No motion detected
}