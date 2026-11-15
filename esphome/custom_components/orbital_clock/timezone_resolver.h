#pragma once

#include <Arduino.h>

class TimezoneResolver {
 public:
  static bool apply(const String &configured, const String &iana_hint = "");
};
