/*
 * Telegram Bot Module - User Notification System
 * 
 * Sends alerts to Telegram chat via Bot API with two delivery methods:
 * 
 * 1. URL Method (for public images):
 *    - Telegram fetches image directly from provided URL
 *    - Fast and simple, minimal ESP32 processing
 * 
 * 2. Multipart Upload Method (for private LAN images):
 *    - ESP32 fetches image bytes from local camera
 *    - Uploads to Telegram via multipart/form-data
 *    - Required because Telegram cannot access private IPs
 * 
 * Includes robust retry logic, timeout handling, and fallback to
 * text-only alerts if image delivery fails.
 */

#include "telegram.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

/*
 * URL-encode a string for safe transmission in HTTP GET parameters
 * 
 * Converts special characters to %XX format (e.g., space -> +)
 * Preserves alphanumeric and safe characters (- _ . ~)
 * 
 * @param str: Raw string to encode
 * @return URL-encoded string safe for HTTP transmission
 */
String urlEncode(String str) {
  String encoded = "";
  char c;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encoded += '+';  // Space becomes +
    } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;  // Safe characters pass through
    } else {
      // Encode as %XX hexadecimal
      encoded += '%';
      char hex[3];
      sprintf(hex, "%02X", c);
      encoded += hex;
    }
  }
  return encoded;
}

/*
 * Send alert to Telegram chat
 * 
 * Two-path delivery strategy:
 * 1. Text-only (if photoURL is empty)
 * 2. Photo URL method (if public URL)
 * 3. Multipart upload method (if private LAN URL)
 * 
 * Process:
 * - Detect if photoURL is private (10.x, 192.168.x, etc.)
 * - If private: fetch image bytes, upload via multipart/form-data
 * - If public: send URL directly to Telegram
 * - If image fetch/upload fails: fallback to text-only alert
 * 
 * @param text: Alert message/caption
 * @param photoURL: Optional image URL or JSON with image location
 */
void Telegram::sendAlert(String text, String photoURL) {
  unsigned long startTime = millis();
  Serial.println("\n=== SENDING TELEGRAM ALERT ===");
  Serial.println("[TELEGRAM] Message: " + text);
  
  HTTPClient http;
  String url;
  
  // Lambda function to detect private IP addresses
  // Returns true if URL points to local/private network (10.x, 192.168.x, 172.16-31.x, 127.x)
  auto isPrivateHttpUrl = [](const String &u) -> bool {
    if (!u.startsWith("http://") && !u.startsWith("https://")) return false;
    // crude host extraction
    int schemeEnd = u.indexOf("://");
    String rest = u.substring(schemeEnd + 3);
    int slash = rest.indexOf('/');
    String host = (slash >= 0) ? rest.substring(0, slash) : rest;
    return host.startsWith("10.") || host.startsWith("192.168.") || host.startsWith("127.") || host.startsWith("172.16.") || host.startsWith("172.17.") || host.startsWith("172.18.") || host.startsWith("172.19.") || host.startsWith("172.20.") || host.startsWith("172.21.") || host.startsWith("172.22.") || host.startsWith("172.23.") || host.startsWith("172.24.") || host.startsWith("172.25.") || host.startsWith("172.26.") || host.startsWith("172.27.") || host.startsWith("172.28.") || host.startsWith("172.29.") || host.startsWith("172.30.") || host.startsWith("172.31.");
  };

  if (photoURL == "") {
    // === Path 1: Text Message Only ===
    Serial.println("[TELEGRAM] Type: Text message only");
    url = "https://api.telegram.org/bot" + String(TELEGRAM_TOKEN)
        + "/sendMessage?chat_id=" + String(TELEGRAM_CHATID)
        + "&text=" + urlEncode(text);
  } else {
    // Parse JSON to extract actual image URL if needed
    String imageUrl = photoURL; // comes from CameraClient::capture() (URL or JSON)
    if (photoURL.startsWith("{")) { // mock mode JSON -> extract url
      Serial.println("[TELEGRAM] Parsing JSON photo response...");
      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, photoURL);
      if (!error && doc.containsKey("url")) {
        imageUrl = doc["url"].as<String>();
        Serial.println("[TELEGRAM] Extracted URL: " + imageUrl);
      }
    }

    // === Check if URL is private/local ===
    // If URL is a private/local address, fetch bytes and upload via multipart
    if (isPrivateHttpUrl(imageUrl)) { // LAN camera -> fetch bytes then upload
      Serial.println("[TELEGRAM] Detected local/private image URL. Uploading bytes via multipart...");

      const int maxFetchAttempts = 3;
      const int maxUploadAttempts = 3;
      const int backoffBaseMs = 600;

      // === STEP 1: FETCH the image with retries ===
      int fetchAttempt = 0;
      int code = -1;
      int contentLength = -1;
      uint8_t* imgBuf = nullptr;
      size_t imgSize = 0;
      
      // Retry loop for fetching image bytes from private URL
      // Attempts up to 3 times with exponential backoff (600ms base)
      do {
        fetchAttempt++;
        HTTPClient imgHttp;
        WiFiClient imgClient;
        
        // Configure HTTP client for image fetching
        imgClient.setTimeout(12000);  // 12-second socket timeout
        imgHttp.setTimeout(20000);    // 20-second overall timeout
        imgHttp.setReuse(false);      // Close connection after use
        imgHttp.useHTTP10(true);      // HTTP/1.0 avoids chunked encoding on some servers
        imgHttp.begin(imgClient, imageUrl); // connect to camera
        imgHttp.addHeader("Connection", "close");
        
        code = imgHttp.GET(); // download JPEG
        Serial.println("[TELEGRAM] Local GET attempt " + String(fetchAttempt) + "/" + String(maxFetchAttempts) + ": " + String(code));
        
        if (code == 200) {
          // Get declared Content-Length from response headers
          contentLength = imgHttp.getSize();
          if (contentLength > 0) {
            Serial.println("[TELEGRAM] Image size (Content-Length): " + String(contentLength) + " bytes");
          } else {
            Serial.println("[TELEGRAM] Image size unknown (chunked/no length)");
          }
          
          WiFiClient *stream = imgHttp.getStreamPtr(); // stream of JPEG data
          
          // Prepare buffer: exact size if known, dynamic if unknown (chunked transfer)
          size_t cap = (contentLength > 0) ? (size_t)contentLength : 8192;
          imgBuf = (uint8_t*)malloc(cap);
          
          if (!imgBuf) {
            Serial.println("[TELEGRAM] ✗ Failed to allocate image buffer");
            imgHttp.end();
          } else {
            size_t readTotal = 0;
            unsigned long lastProgress = millis();
            const unsigned long maxIdleMs = 12000; // 12-second idle timeout
            
            // Stream reading loop with idle timeout and dynamic buffer growth
            while (true) {
              int avail = stream->available();
              
              if (avail <= 0) {
                // No data available - check if still connected and not timed out
                if (imgClient.connected() && (millis() - lastProgress) < maxIdleMs) {
                  delay(20); // Small delay to avoid busy-waiting
                  continue;
                }
                // Connection closed or idle timeout - exit loop
                break;
              }
              
              size_t toRead = avail;
              
              if (contentLength > 0) {
                // Known length: don't read beyond declared size
                size_t remaining = (size_t)contentLength - readTotal;
                if (toRead > (int)remaining) toRead = remaining;
              } else {
                // Unknown length (chunked): grow buffer dynamically if needed
                if (readTotal + toRead > cap) {
                  size_t newCap = cap * 2;  // Double capacity
                  if (newCap < readTotal + toRead) newCap = readTotal + toRead;
                  uint8_t* nb = (uint8_t*)realloc(imgBuf, newCap);
                  
                  if (!nb) {
                    Serial.println("[TELEGRAM] ✗ Failed to grow buffer");
                    break;
                  }
                  imgBuf = nb;
                  cap = newCap;
                }
              }
              
              // Read chunk into buffer
              int n = stream->read(imgBuf + readTotal, toRead);
              if (n > 0) {
                readTotal += n;
                lastProgress = millis(); // Reset idle timer
              } else {
                delay(10); // Small wait to allow more data
              }
              
              // Check if finished reading exact length
              if (contentLength > 0 && readTotal == (size_t)contentLength) {
                break;
              }
            }
            
            imgHttp.end();
            imgSize = readTotal;
            
            // Validate completeness when length was known
            if (contentLength > 0 && imgSize != (size_t)contentLength) {
              Serial.println("[TELEGRAM] ✗ Short read: " + String(imgSize) + "/" + String(contentLength));
              free(imgBuf); imgBuf = nullptr; imgSize = 0;
            } else if (imgSize == 0) {
              Serial.println("[TELEGRAM] ✗ No data read from stream");
              free(imgBuf); imgBuf = nullptr;
            } else {
              Serial.println("[TELEGRAM] ✓ Image fetched successfully (" + String(imgSize) + " bytes)");
              break; // Success - exit retry loop
            }
          }
        } else {
          imgHttp.end();
        }
        
        // Retry backoff delay (exponential: 600ms, 1200ms, 1800ms)
        if (fetchAttempt < maxFetchAttempts) {
          int backoff = backoffBaseMs * fetchAttempt;
          Serial.println("[TELEGRAM] Retrying fetch in " + String(backoff) + "ms...");
          delay(backoff);
        }
      } while (fetchAttempt < maxFetchAttempts);

      if (code == 200 && imgBuf && imgSize > 0) {
        // === STEP 2: UPLOAD via multipart/form-data with retries ===
        
        // Generate unique boundary string for multipart form
        String boundary = "----ESP32Boundary" + String(millis()); // multipart boundary
        
        // Build multipart form body parts
        // Part 1: chat_id field
        String pre = "--" + boundary + "\r\n"
                      "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(TELEGRAM_CHATID) + "\r\n"
                      "--" + boundary + "\r\n"
                      // Part 2: caption field
                      "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" + text + "\r\n"
                      "--" + boundary + "\r\n"
                      // Part 3: photo file field
                      "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n"
                      "Content-Type: image/jpeg\r\n\r\n";
        String post = "\r\n--" + boundary + "--\r\n";
        
        // Calculate total body size: header + image + footer
        size_t totalLen = pre.length() + imgSize + post.length();

        // Allocate single buffer for complete multipart body
        std::unique_ptr<uint8_t[]> body(new (std::nothrow) uint8_t[totalLen]);
        if (!body) {
          Serial.println("[TELEGRAM] ✗ Failed to allocate multipart body (" + String(totalLen) + " bytes)");
          free(imgBuf);
        } else {
          // Assemble complete multipart body in memory
          memcpy(body.get(), pre.c_str(), pre.length());                           // Header
          memcpy(body.get() + pre.length(), imgBuf, imgSize);                      // Image bytes
          memcpy(body.get() + pre.length() + imgSize, post.c_str(), post.length()); // Footer

          int uploadAttempt = 0;
          int upCode = -1;
          String resp;
          
          // Retry loop for uploading multipart form to Telegram
          // Attempts up to 3 times with exponential backoff
          do {
            uploadAttempt++;
            String apiUrl = "https://api.telegram.org/bot" + String(TELEGRAM_TOKEN) + "/sendPhoto";
            
            HTTPClient upHttp;
            upHttp.setTimeout(20000); // 20-second timeout for upload
            upHttp.begin(apiUrl);
            upHttp.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
            
            // POST entire multipart body
            upCode = upHttp.POST(body.get(), totalLen); // upload to Telegram
            Serial.println("[TELEGRAM] Upload attempt " + String(uploadAttempt) + "/" + String(maxUploadAttempts) + ": " + String(upCode));
            
            if (upCode != 200) {
              // Upload failed - log response and retry
              resp = upHttp.getString();
              Serial.println("[TELEGRAM] Response: " + resp);
              upHttp.end();
              
              if (uploadAttempt < maxUploadAttempts) {
                int backoff = backoffBaseMs * uploadAttempt;
                Serial.println("[TELEGRAM] Retrying upload in " + String(backoff) + "ms...");
                delay(backoff);
              }
            } else {
              // Upload successful
              Serial.println("[TELEGRAM] ✓ Photo uploaded successfully");
              upHttp.end();
              free(imgBuf);
              
              unsigned long elapsed = millis() - startTime;
              Serial.println("[PERF] Telegram send took: " + String(elapsed) + "ms");
              return; // Exit function - photo delivered successfully
            }
          } while (uploadAttempt < maxUploadAttempts);
          
          free(imgBuf); // Clean up after failed upload attempts
        }
      } else {
        Serial.println("[TELEGRAM] ✗ Failed to fetch local image for upload after retries");
      }
      // Fallback to URL method below if multipart upload failed
    }

    // === Path 2: Photo URL Method (for public URLs or multipart fallback) ===
    Serial.println("[TELEGRAM] Type: Photo with caption via URL");
    Serial.println("[TELEGRAM] Photo URL: " + imageUrl);
    url = "https://api.telegram.org/bot" + String(TELEGRAM_TOKEN) // public URL path
        + "/sendPhoto?chat_id=" + String(TELEGRAM_CHATID)
        + "&photo=" + urlEncode(imageUrl)
        + "&caption=" + urlEncode(text);
  }
  
  // === Final API call (text-only or URL-based photo) ===
  Serial.println("[TELEGRAM] Sending request to Telegram API...");
  http.begin(url);
  int httpCode = http.GET();
  
  Serial.print("[TELEGRAM] Response code: ");
  Serial.println(httpCode);
  
  if (httpCode == 200) {
    Serial.println("[TELEGRAM] ✓ Alert sent successfully");
  } else {
    Serial.println("[TELEGRAM] ✗ Failed to send alert");
    String response = http.getString();
    Serial.println("[TELEGRAM] Response: " + response);
  }
  
  unsigned long elapsed = millis() - startTime;
  Serial.println("[PERF] Telegram send took: " + String(elapsed) + "ms");
  
  http.end();
}