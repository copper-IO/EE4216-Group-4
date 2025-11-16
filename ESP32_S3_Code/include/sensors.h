#pragma once
#include <Arduino.h>
struct SensorData {
  float temp, hum;
  String ts;
};
namespace Sensors {
  void init();
  SensorData readAll();
}