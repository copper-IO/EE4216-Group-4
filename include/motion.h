#pragma once
#include <Arduino.h>
namespace Motion {
  void init(int pin);
  bool motionDetected();
}