#pragma once

#include <Arduino.h>

class TimezoneResolver {
 public:
  static const char *posix_for(const String &iana);
  static String resolve(const String &configured, const String &iana_hint);
};
