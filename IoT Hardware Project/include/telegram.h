#pragma once
#include <Arduino.h>
namespace Telegram {
  void sendAlert(String text, String photoURL="");
}