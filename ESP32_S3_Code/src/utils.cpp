/*
 * Utility Functions Module
 * 
 * Provides common helper functions used across the system.
 * Currently includes timestamp generation for logging and alerts.
 */

#include "utils.h"
#include <time.h>

/*
 * Generate ISO 8601 formatted timestamp
 * 
 * Returns current time in format: YYYY-MM-DDTHH:MM:SS
 * Example: "2025-01-15T14:30:45"
 * 
 * Used for:
 * - Alert message timestamps
 * - Log entries with time reference
 * - Event tracking in cloud dashboard
 * 
 * Note: Requires NTP time sync during WiFi setup
 */
String Utils::timestamp() {
  time_t now;
  time(&now); // Get current Unix timestamp
  
  struct tm t;
  localtime_r(&now, &t); // Convert to local time structure
  
  char buf[25];
  // Format as ISO 8601: YYYY-MM-DDTHH:MM:SS
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &t);
  
  return String(buf);
}