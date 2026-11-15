#include "../custom_components/orbital_clock/display_schedule.h"

using namespace display_schedule;

static_assert(is_night(22 * 3600, 22 * 3600, 7 * 3600));
static_assert(is_night(0, 22 * 3600, 7 * 3600));
static_assert(is_night(7 * 3600 - 1, 22 * 3600, 7 * 3600));
static_assert(!is_night(7 * 3600, 22 * 3600, 7 * 3600));
static_assert(!is_night(22 * 3600 - 1, 22 * 3600, 7 * 3600));
static_assert(!is_night(11 * 3600, 12 * 3600, 18 * 3600));
static_assert(is_night(12 * 3600, 12 * 3600, 18 * 3600));
static_assert(!is_night(18 * 3600, 12 * 3600, 18 * 3600));
static_assert(!is_night(12 * 3600, 12 * 3600, 12 * 3600));
static_assert(contrast(0) == 0 && contrast(12) == 30 && contrast(100) == 255);
static_assert(!rotation_due(60000, 0, 0));
static_assert(!rotation_due(59999, 0, 60000));
static_assert(rotation_due(60000, 0, 60000));
static_assert(rotation_due(10, UINT32_MAX - 9, 20));
