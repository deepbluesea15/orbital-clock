#pragma once

#include <stdint.h>

class BounceMotion {
 public:
  constexpr void step(float max_x, float max_y, uint32_t random_value) {
    if (!initialized_) {
      vx_ = random_value & 1 ? 1.0f : -1.0f;
      vy_ = random_value & 2 ? 1.0f : -1.0f;
      vary_velocity(random_value >> 2);
      initialized_ = true;
    }

    x_ += vx_;
    y_ += vy_;
    bool hit_x = false;
    bool hit_y = false;

    if (aiming_ && (vx_ > 0 ? x_ >= target_x_ : x_ <= target_x_)) {
      x_ = target_x_;
      y_ = target_y_;
      vx_ = -vx_;
      vy_ = -vy_;
      aiming_ = false;
      hit_x = hit_y = true;
    } else if (aiming_) {
      if (x_ < 0) x_ = 0;
      if (x_ > max_x) x_ = max_x;
      if (y_ < 0) y_ = 0;
      if (y_ > max_y) y_ = max_y;
    } else {
      if (x_ <= 0) { x_ = 0; vx_ = -vx_; hit_x = true; }
      if (x_ >= max_x) { x_ = max_x; vx_ = -vx_; hit_x = true; }
      if (y_ <= 0) { y_ = 0; vy_ = -vy_; hit_y = true; }
      if (y_ >= max_y) { y_ = max_y; vy_ = -vy_; hit_y = true; }
    }

    if (hit_x || hit_y) {
      ++hits_;
      if (hit_x && hit_y) ++corners_;
      vary_velocity(random_value);
      if (!(hit_x && hit_y) && (random_value / 72) % 25 == 0) {
        // Aim across both axes so the next collision reaches an actual corner.
        target_x_ = x_ < max_x / 2 ? max_x : 0;
        target_y_ = y_ < max_y / 2 ? max_y : 0;
        float dx = target_x_ - x_;
        float dy = target_y_ - y_;
        float distance_x = dx < 0 ? -dx : dx;
        float distance_y = dy < 0 ? -dy : dy;
        float steps = (distance_x > distance_y ? distance_x : distance_y) / 1.2f;
        vx_ = dx / steps;
        vy_ = dy / steps;
        aiming_ = true;
      }
    }
  }

  constexpr float x() const { return x_; }
  constexpr float y() const { return y_; }
  constexpr uint16_t hits() const { return hits_; }
  constexpr uint16_t corners() const { return corners_; }

 private:
  constexpr void vary_velocity(uint32_t random_value) {
    float speed_x = 0.85f + (random_value % 8) * 0.1f;
    float speed_y = 0.6f + ((random_value / 8) % 9) * 0.1f;
    vx_ = vx_ < 0 ? -speed_x : speed_x;
    vy_ = vy_ < 0 ? -speed_y : speed_y;
  }

  float x_ = 20, y_ = 20;
  float vx_ = 1, vy_ = 1;
  float target_x_ = 0, target_y_ = 0;
  uint16_t hits_ = 0, corners_ = 0;
  bool initialized_ = false;
  bool aiming_ = false;
};
