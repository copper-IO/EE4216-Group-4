#pragma once
#include <Arduino.h>
namespace CameraClient {
  String capture();
  String captureMock(); // Mock camera for testing
  void setMockMode(bool enabled);
  bool isMockMode();
  bool checkConnection(); // Check if camera is online
}