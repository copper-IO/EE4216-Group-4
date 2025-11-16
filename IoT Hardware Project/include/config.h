#pragma once
#include <Arduino.h>

// ---- Wi-Fi ----
#define WIFI_SSID       "Copper"
#define WIFI_PASS       "mockPassword123"

// ---- Adafruit IO ----
#define IO_USERNAME     "CopperIO"
#define IO_KEY          "aio_mockAPIKey1234567890abcdef"

// ---- Telegram ----
#define TELEGRAM_TOKEN  "8465496106:AAHR_mockToken1234567890abcdef"
#define TELEGRAM_CHATID "1111111111" //mockChatID

// ---- Thresholds ----
#define TEMP_LIMIT      34.0
#define HUM_LIMIT       90.0

// ---- Pins ----
#define DHTPIN          4
#define DHTTYPE         DHT22
#define PIRPIN          15  // Changed from 36 - ESP32-S3 only supports interrupts on GPIO 0-21

// ---- Camera Node ----
#define CAM_IP          "10.28.158.71"  // ESP32-CAM local IP
#define CAM_PORT        80
