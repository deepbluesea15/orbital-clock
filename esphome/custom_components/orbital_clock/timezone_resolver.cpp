#include "timezone_resolver.h"

namespace {

struct TimezoneEntry {
  const char *iana;
  const char *posix;
};

static const TimezoneEntry TIMEZONES[] = {
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Detroit", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Phoenix", "MST7"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Vancouver", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"Pacific/Honolulu", "HST10"},
    {"America/Mexico_City", "CST6"},
    {"America/Sao_Paulo", "<-03>3"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0"},
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Warsaw", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Moscow", "MSK-3"},
    {"Asia/Dubai", "<+04>-4"},
    {"Asia/Kolkata", "IST-5:30"},
    {"Asia/Bangkok", "<+07>-7"},
    {"Asia/Singapore", "<+08>-8"},
    {"Asia/Shanghai", "CST-8"},
    {"Asia/Hong_Kong", "HKT-8"},
    {"Asia/Tokyo", "JST-9"},
    {"Asia/Seoul", "KST-9"},
    {"Australia/Perth", "AWST-8"},
    {"Australia/Brisbane", "AEST-10"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"UTC", "UTC0"},
};

}  // namespace

const char *TimezoneResolver::posix_for(const String &iana) {
  for (const auto &timezone : TIMEZONES) {
    if (iana == timezone.iana) return timezone.posix;
  }
  return "UTC0";
}

String TimezoneResolver::resolve(const String &configured, const String &iana_hint) {
  if (configured.length() && configured != "auto") return configured;
  if (iana_hint.length()) return String(posix_for(iana_hint));
  return "UTC0";
}
