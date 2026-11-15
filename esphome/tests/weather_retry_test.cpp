#include "../custom_components/orbital_clock/weather_retry.h"

using namespace weather_retry;

static_assert(next_delay(INITIAL_MS) == 120000);
static_assert(next_delay(240000) == 480000);
static_assert(next_delay(480000) == MAX_MS);
static_assert(next_delay(MAX_MS) == MAX_MS);
static_assert(jittered_delay(INITIAL_MS, 0) == 60000);
static_assert(jittered_delay(INITIAL_MS, 6000) == 66000);
static_assert(jittered_delay(480000, 48000) == 528000);
static_assert(jittered_delay(MAX_MS, 0) == MAX_MS);
static_assert(jittered_delay(MAX_MS, 90000) == 810000);
