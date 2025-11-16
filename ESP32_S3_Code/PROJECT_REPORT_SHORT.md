Project Title: IoT Smart Home Security System
Team: [Your Names]
Course: [Course Code]
Instructor: [Instructor Name]
Date: November 16, 2025
GitHub: https://github.com/your-repo (replace with your actual repo URL)

---

Project Overview

This project implements a low-cost IoT smart home security system that combines environmental monitoring (temperature and humidity), motion detection, and photographic evidence capturing to deliver timely alerts. The system runs on an ESP32-S3 microcontroller, reads a DHT22 sensor every 30 seconds, uses a PIR motion sensor with interrupt-based detection, and integrates a networked camera (or mock mode) for imagery. Alerts are published to Adafruit IO (MQTT) for dashboarding and delivered via a Telegram bot for immediate notification.

Objectives
- Continuously monitor temperature and humidity and publish data to Adafruit IO.
- Detect motion reliably and capture an associated image when possible.
- Deliver timely alerts to a Telegram chat and publish alert events to MQTT.
- Be robust to network and camera failures and be extensible for future enhancements.

Scope and Constraints
- Does not store long-term historical data on the device; cloud/dashboarding is used for visualization.
- Uses WiFi and assumes connectivity to the local network for camera and cloud APIs.
- Credentials are stored in configuration headers during development; production deployment should use secure storage.

Design Concepts

System Architecture
The system is modular with separations for sensing, motion handling, camera capture, alert coordination, cloud publishing, and notification. Two FreeRTOS tasks are pinned to the second core: a SensorTask that handles periodic reads and publishing, and an AlertTask that monitors motion flags and orchestrates image capture and alerts. The main core performs minimal work (startup and periodic health checks) to avoid blocking.

Hardware and Interfaces
- DHT22 provides temperature and humidity data (GPIO 4).
- PIR motion sensor is attached with interrupt support (GPIO 15) and uses a 5-second debounce to suppress spurious triggers.
- Camera (optional) is accessed over HTTP and can run in real or mock mode for development.

Communication Protocols
- MQTT (Adafruit IO) is used for publishing temperature, humidity, and alert messages. Connection attempts include retry logic and the publishing flow respects rate limits.
- Telegram Bot API is used for human-facing alerts. For private/local camera image URLs, the system fetches image bytes and uploads via multipart/form-data; otherwise the system sends a photo URL.

State & Fault Handling
- Temperature and humidity alerts use a simple state machine: send one alert when a threshold is crossed and reset when conditions return to normal.
- Motion alerts employ a 60-second cooldown to avoid spamming notifications.
- Camera connection is checked at startup; mock mode allows continued development when hardware is unavailable.

Implementation Summary (no raw code included)

Initialization and Startup
On boot the device initializes serial logging, connects to WiFi with timeouts and restart behavior on repeated failures, initializes the sensor and MQTT client, checks camera availability, and then starts the two FreeRTOS tasks described above.

Sensor Task
The SensorTask reads the DHT22 sensor every 30 seconds, sanitizes values (skips NaNs), adds a timestamp, and publishes temperature and humidity values to separate Adafruit IO feeds with spacing to respect rate limits. Weather alert checks occur after each reading.

Motion and Alert Task
The motion sensor triggers an ISR which sets a volatile flag; the AlertTask polls this flag each second. When motion is detected outside the cooldown window, the system captures an image (real or mock), sends a Telegram alert (photo + caption where available), and publishes a textual alert event to MQTT. Performance logging is included to measure latency.

Network & Notification Handling
MQTT includes a controlled reconnect strategy and graceful degradation if offline. Telegram handling includes URL encoding for messages and a robust multipart upload path for images hosted on private IPs. The implementation avoids sending large image payloads over MQTT; images are sent to Telegram while shorter alert messages go to the dashboard feed.

Implementation Challenges and Solutions

Challenge 1 — Motion Sensor Oversensitivity
- Symptoms: Frequent false triggers without real motion, multiple alerts per event, chatter during PIR warm‑up.
- Actions taken: (1) Tuned the PIR module (lower sensitivity, minimum off‑time), (2) ensured a defined idle level by enabling an internal pull‑down on the ESP32‑S3 input, (3) used a RISING‑edge interrupt only, (4) added a 5‑second software debounce to ignore rapid re‑triggers, and (5) added a 60‑second alert‑layer cooldown to prevent notification spam. Motion detected during the first minute after boot is also suppressed to cover PIR warm‑up.
- Outcome: False alerts dropped significantly; one clear alert per event with photo, improved user experience and data quality.

Challenge 2 — Image Fetching to Telegram
- Initial approach: Considered converting the camera image to Base64 and sending it in a message body. This was rejected because the Telegram Bot API does not accept Base64 blobs for photos; it expects either (a) a publicly reachable URL or (b) a file upload via multipart/form‑data. Base64 would also inflate payload size (~33% overhead) and strain memory/CPU on the microcontroller.
- Constraint discovered: Our camera is on a private LAN (e.g., 10.x / 192.168.x). Telegram’s servers cannot fetch private URLs, so simply sending a private link would fail.
- Final design: Two‑path strategy in the Telegram sender.
	1) Public URL path: If the photo URL is publicly reachable, send it directly using Telegram’s photo‑by‑URL endpoint with a caption.
	2) Private URL upload path: If the URL is private, the controller first fetches the image bytes locally from the camera, then uploads those bytes to Telegram using multipart/form‑data (chat_id, caption, and the JPEG payload). This turns the device into an “uploader proxy,” ensuring Telegram can receive the image even when the camera is not internet‑addressable.
- Robustness features implemented: Private‑URL detection, bounded timeouts, multiple fetch and upload retries with exponential backoff, support for unknown Content‑Length (chunked transfer) via dynamic buffer growth with idle timeouts, and explicit resource cleanup to avoid memory leaks. If fetching/uploading ultimately fails, the system falls back to sending a text‑only alert so the user is still notified.
- Rationale: Complies with Telegram API expectations, avoids MQTT payload limits and Adafruit IO rate/size constraints, and guarantees photo delivery regardless of local network visibility.
- Outcome: Reliable photo alerts in real‑world conditions with private cameras; typical end‑to‑end photo alert completes in a few seconds depending on network conditions.

Challenge 3 — Sensor Sampling Timing Drift
- Problem: Initially used `vTaskDelay()` which makes tasks sleep for a fixed duration after work completes. Since the actual period = work_time + delay_time, and work time varies (sensor reads ~200ms, MQTT publish ~1-2s depending on network), the actual sampling interval drifted from the intended 30 seconds to 32-33 seconds per cycle.
- Impact: Over extended operation this causes cumulative drift in timestamps and reduces precision for time-series analysis on the cloud dashboard. The system was not maintaining a true fixed sampling rate.
- Solution: Replaced `vTaskDelay(pdMS_TO_TICKS(30000))` with `vTaskDelayUntil(&xLastWakeTime, xPeriod)` in the SensorTask. This FreeRTOS API maintains an absolute wake time tracked in `xLastWakeTime` and automatically advances it by `xPeriod` each cycle, ensuring the task wakes at exact 30-second intervals regardless of how long the work takes.
- Implementation details: Added a `TickType_t xLastWakeTime` initialized to the current tick count at task start, and a constant `xPeriod` of 30000ms. The scheduler now wakes the task at precise intervals, compensating for variable work duration.
- Outcome: Sensor readings now occur at exactly 30-second intervals with no cumulative drift, improving data quality for cloud analytics and ensuring compliance with the intended sampling rate.

Results & Evaluation

Key metrics observed from testing and serial logs:
- Sensor cycle frequency: ~30 seconds as designed.
- Motion detection latency: empirical typical latency from physical event to alert initiation is on the order of a few hundred milliseconds; total alert cycle depends on network latency and image upload (~1–4 seconds observed).
- Alerts behavior: Weather alerts fire once on threshold exceedance (temperature threshold set at 34°C, humidity threshold 90%) and reset when conditions normalize. Motion alerts respect a 60-second cooldown.

Reliability Observations
- The system continues to function when MQTT is temporarily unavailable (it retries on next cycle) and continues sensor reporting. If the camera is offline, mock mode or omission of images ensures the system still sends textual alerts.

Limitations
- Thresholds are compile-time constants; no runtime configuration currently.
- Credentials are in source during development and should be moved to secure storage for deployment.
- No persistent local queue for failed alerts across power cycles.

Conclusion

The implemented system successfully meets all seven specified project objectives: environmental monitoring (DHT22 for temperature and humidity), security monitoring (PIR motion detection), multitasking (FreeRTOS dual-core tasks), Wi-Fi connectivity (Adafruit IO and Telegram), data logging and visualization (cloud dashboards), real-time alerts (motion and threshold-based notifications with photos), and local network access (camera HTTP endpoint).

Key technical solutions addressed motion sensor oversensitivity through hardware tuning and software debouncing, implemented two-path image delivery for private LAN cameras via multipart upload, and eliminated sampling drift using fixed-period scheduling with `vTaskDelayUntil`. The modular architecture provides a maintainable foundation suitable for extensions including runtime configuration, secure credential storage, OTA updates, and integration with home automation platforms.

Future Improvements
- Direct ESP32-CAM upload: Currently the main ESP32-S3 controller fetches images from the camera and proxies them to Telegram. A more efficient approach would enable the ESP32-CAM to upload photos directly to Telegram or a cloud storage service, reducing latency and offloading network traffic from the main controller.
- On-demand sensor queries: Implement a command interface (via Telegram bot commands or HTTP API) allowing users to request current sensor readings on demand rather than waiting for the next 30-second cycle, providing immediate feedback for manual verification.
- Additional enhancements: Runtime threshold configuration, persistent alert queue across reboots, battery backup with low-power modes, multiple camera/sensor node support, and integration with voice assistants or home automation platforms.

Camera Section — To be completed by the camera teammate
(Please supply the following details: hardware pinout and wiring, camera firmware settings, HTTP endpoints used, sample image URL/JSON format, capture latency measurements and statistics, image size distribution, image quality notes, and failure/recovery steps.)

References

[1] Adafruit DHT sensor library v1.4.6. Available: https://github.com/adafruit/DHT-sensor-library

[2] Adafruit MQTT Library v2.5.9. Available: https://github.com/adafruit/Adafruit_MQTT_Library

[3] ArduinoJson v6.21.3. Available: https://github.com/bblanchon/ArduinoJson

---

Notes for submission
- Use Times New Roman, 12 pt, double-spaced formatting when converting this text into your final report document; this file is a compact draft to paste into your Word/PDF report. Do not include raw source files in the report; provide the GitHub link instead. Ensure your GitHub repo contains well-commented source code and a README explaining how to run and test the system.