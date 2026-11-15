#include "../custom_components/orbital_clock_faces/bounce_motion.h"

constexpr bool bounded_with_real_corner_hits() {
  BounceMotion motion;
  for (int i = 0; i < 1000; i++) {
    uint16_t previous_hits = motion.hits();
    uint16_t previous_corners = motion.corners();
    motion.step(67, 46, 0);
    if (motion.x() < 0 || motion.x() > 67 || motion.y() < 0 || motion.y() > 46) return false;
    if (motion.hits() > previous_hits + 1 || motion.corners() > previous_corners + 1) return false;
    if (motion.corners() != previous_corners &&
        ((motion.x() != 0 && motion.x() != 67) || (motion.y() != 0 && motion.y() != 46) ||
         motion.hits() != previous_hits + 1)) return false;
  }
  return motion.hits() > 5 && motion.corners() > 1;
}

constexpr bool different_initial_directions() {
  BounceMotion first;
  BounceMotion second;
  first.step(67, 46, 0);
  second.step(67, 46, 3);
  return first.x() < 20 && first.y() < 20 && second.x() > 20 && second.y() > 20;
}

constexpr bool varied_rebounds_stay_in_bounds() {
  for (uint32_t seed = 1; seed <= 4; seed++) {
    BounceMotion motion;
    uint32_t random_value = seed;
    for (int i = 0; i < 4000; i++) {
      random_value = random_value * 1664525u + 1013904223u;
      uint16_t corners = motion.corners();
      motion.step(67, 46, random_value);
      if (motion.x() < 0 || motion.x() > 67 || motion.y() < 0 || motion.y() > 46) return false;
      if (motion.corners() != corners &&
          ((motion.x() != 0 && motion.x() != 67) || (motion.y() != 0 && motion.y() != 46))) return false;
    }
    if (motion.hits() < 10 || motion.corners() == 0) return false;
  }
  return true;
}

static_assert(bounded_with_real_corner_hits());
static_assert(different_initial_directions());
static_assert(varied_rebounds_stay_in_bounds());
