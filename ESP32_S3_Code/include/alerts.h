#pragma once
#include <Arduino.h>
#include "sensors.h"

class Alerts {
public:
  static void checkWeatherAlerts(SensorData data);
  static void handleMotionAlert(String photoURL);
  
private:
  static bool tempAlertSent;
  static bool humAlertSent;
};
