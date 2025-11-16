#pragma once
#include "sensors.h"
namespace NetMQTT {
  void init();
  void publishEnv(SensorData d);
  void publishAlert(String reason, String photoURL);
}