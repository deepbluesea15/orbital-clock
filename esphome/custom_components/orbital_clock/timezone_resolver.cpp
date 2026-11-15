#include "timezone_resolver.h"

#include "esphome/core/defines.h"
#include "esphome/core/time.h"
#include "esphome/components/time/posix_tz.h"

namespace {

using esphome::time::DSTRule;
using esphome::time::DSTRuleType;
using esphome::time::ParsedTimezone;

constexpr DSTRule no_dst{};
constexpr DSTRule transition(uint8_t month, uint8_t week, int32_t seconds = 7200) {
  return {seconds, 0, DSTRuleType::MONTH_WEEK_DAY, month, week, 0};
}
constexpr ParsedTimezone fixed_zone(int32_t standard_offset) {
  return {standard_offset, 0, no_dst, no_dst};
}
constexpr ParsedTimezone seasonal_zone(int32_t standard_offset, int32_t daylight_offset, DSTRule start,
                                       DSTRule end) {
  return {standard_offset, daylight_offset, start, end};
}

constexpr ParsedTimezone UTC = fixed_zone(0);
constexpr ParsedTimezone EASTERN = seasonal_zone(18000, 14400, transition(3, 2), transition(11, 1));
constexpr ParsedTimezone CENTRAL = seasonal_zone(21600, 18000, transition(3, 2), transition(11, 1));
constexpr ParsedTimezone MOUNTAIN = seasonal_zone(25200, 21600, transition(3, 2), transition(11, 1));
constexpr ParsedTimezone PACIFIC = seasonal_zone(28800, 25200, transition(3, 2), transition(11, 1));
constexpr ParsedTimezone ALASKA = seasonal_zone(32400, 28800, transition(3, 2), transition(11, 1));
constexpr ParsedTimezone ARIZONA = fixed_zone(25200);
constexpr ParsedTimezone HAWAII = fixed_zone(36000);
constexpr ParsedTimezone MEXICO = fixed_zone(21600);
constexpr ParsedTimezone SAO_PAULO = fixed_zone(10800);
constexpr ParsedTimezone WEST_EUROPE = seasonal_zone(0, -3600, transition(3, 5, 3600), transition(10, 5));
constexpr ParsedTimezone CENTRAL_EUROPE = seasonal_zone(-3600, -7200, transition(3, 5), transition(10, 5, 10800));
constexpr ParsedTimezone EAST_EUROPE =
    seasonal_zone(-7200, -10800, transition(3, 5, 10800), transition(10, 5, 14400));
constexpr ParsedTimezone MOSCOW = fixed_zone(-10800);
constexpr ParsedTimezone DUBAI = fixed_zone(-14400);
constexpr ParsedTimezone KOLKATA = fixed_zone(-19800);
constexpr ParsedTimezone BANGKOK = fixed_zone(-25200);
constexpr ParsedTimezone EAST_ASIA = fixed_zone(-28800);
constexpr ParsedTimezone JAPAN = fixed_zone(-32400);
constexpr ParsedTimezone PERTH = fixed_zone(-28800);
constexpr ParsedTimezone BRISBANE = fixed_zone(-36000);
constexpr ParsedTimezone SYDNEY = seasonal_zone(-36000, -39600, transition(10, 1), transition(4, 1, 10800));
constexpr ParsedTimezone AUCKLAND = seasonal_zone(-43200, -46800, transition(9, 5), transition(4, 1, 10800));

struct TimezoneEntry {
  const char *iana;
  const ParsedTimezone *timezone;
};

const TimezoneEntry TIMEZONES[] = {
    {"America/New_York", &EASTERN},
    {"America/Detroit", &EASTERN},
    {"America/Toronto", &EASTERN},
    {"America/Chicago", &CENTRAL},
    {"America/Denver", &MOUNTAIN},
    {"America/Phoenix", &ARIZONA},
    {"America/Los_Angeles", &PACIFIC},
    {"America/Vancouver", &PACIFIC},
    {"America/Anchorage", &ALASKA},
    {"Pacific/Honolulu", &HAWAII},
    {"America/Mexico_City", &MEXICO},
    {"America/Sao_Paulo", &SAO_PAULO},
    {"Europe/London", &WEST_EUROPE},
    {"Europe/Lisbon", &WEST_EUROPE},
    {"Europe/Paris", &CENTRAL_EUROPE},
    {"Europe/Berlin", &CENTRAL_EUROPE},
    {"Europe/Madrid", &CENTRAL_EUROPE},
    {"Europe/Rome", &CENTRAL_EUROPE},
    {"Europe/Amsterdam", &CENTRAL_EUROPE},
    {"Europe/Warsaw", &CENTRAL_EUROPE},
    {"Europe/Athens", &EAST_EUROPE},
    {"Europe/Helsinki", &EAST_EUROPE},
    {"Europe/Kyiv", &EAST_EUROPE},
    {"Europe/Moscow", &MOSCOW},
    {"Asia/Dubai", &DUBAI},
    {"Asia/Kolkata", &KOLKATA},
    {"Asia/Bangkok", &BANGKOK},
    {"Asia/Singapore", &EAST_ASIA},
    {"Asia/Shanghai", &EAST_ASIA},
    {"Asia/Hong_Kong", &EAST_ASIA},
    {"Asia/Tokyo", &JAPAN},
    {"Asia/Seoul", &JAPAN},
    {"Australia/Perth", &PERTH},
    {"Australia/Brisbane", &BRISBANE},
    {"Australia/Sydney", &SYDNEY},
    {"Pacific/Auckland", &AUCKLAND},
    {"UTC", &UTC},
};

}  // namespace

bool TimezoneResolver::apply(const String &configured, const String &iana_hint) {
  const String &iana =
      configured.length() && configured != "Automatic" && configured != "auto" ? configured : iana_hint;
  if (!iana.length()) {
    esphome::time::set_global_tz(UTC);
    return false;
  }
  for (const auto &timezone : TIMEZONES) {
    if (iana == timezone.iana) {
      esphome::time::set_global_tz(*timezone.timezone);
      return true;
    }
  }
  esphome::time::set_global_tz(UTC);
  return false;
}
