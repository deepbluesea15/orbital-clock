#pragma once

class ClockFace {
 public:
  virtual ~ClockFace() = default;
  virtual void draw() = 0;
};
