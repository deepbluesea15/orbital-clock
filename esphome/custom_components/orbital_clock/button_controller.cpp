#include "button_controller.h"

#include <Arduino.h>

#include "clock_face_context.h"
#include "clock_runtime.h"

namespace {

constexpr uint8_t PIN_BTN = 0;
constexpr uint32_t HOLD_FORMAT_MS = 800;
constexpr uint32_t HOLD_RESET_MS = 8000;
constexpr uint32_t HOLD_WARN_MS = 1800;

}  // namespace

uint32_t ButtonController::held_ms() const {
  return this->button_down_ ? millis() - this->button_down_at_ : 0;
}

void ButtonController::next_face() { orbital_next_face(); }

void ButtonController::toggle_time_format() { orbital_toggle_time_format(); }

void ButtonController::factory_reset() {
  orbital_reset_settings();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  const char *reset = "SETTINGS RESET";
  const char *restart = "RESTARTING";
  u8g2.drawStr((W - u8g2.getStrWidth(reset)) / 2, 28, reset);
  u8g2.drawStr((W - u8g2.getStrWidth(restart)) / 2, 42, restart);
  u8g2.sendBuffer();
  delay(1400);
  ESP.restart();
}

void ButtonController::handle() {
  bool pressed = digitalRead(PIN_BTN) == LOW;

  if (pressed && !this->button_down_) {
    this->button_down_ = true;
    this->button_down_at_ = millis();
    this->long_fired_ = false;
  } else if (pressed && this->button_down_) {
    uint32_t held = millis() - this->button_down_at_;
    if (!this->long_fired_ && held > HOLD_FORMAT_MS) {
      this->long_fired_ = true;
      this->toggle_time_format();
    }
    if (held >= HOLD_RESET_MS) this->factory_reset();
  } else if (!pressed && this->button_down_) {
    this->button_down_ = false;
    if (!this->long_fired_ && millis() - this->button_down_at_ > 40) this->next_face();
  }
}

void ButtonController::draw_reset_overlay() {
  uint32_t held = this->held_ms();
  if (held < HOLD_WARN_MS) return;

  float progress = static_cast<float>(held - HOLD_WARN_MS) / (HOLD_RESET_MS - HOLD_WARN_MS);
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  int seconds_left = static_cast<int>((HOLD_RESET_MS - held + 999) / 1000);
  if (seconds_left < 0) seconds_left = 0;

  u8g2.setDrawColor(0);
  u8g2.drawBox(4, 16, 120, 32);
  u8g2.setDrawColor(1);
  u8g2.drawFrame(4, 16, 120, 32);

  u8g2.setFont(u8g2_font_5x7_tf);
  const char *message = "KEEP HOLDING TO RESET";
  u8g2.drawStr((W - u8g2.getStrWidth(message)) / 2, 27, message);

  u8g2.drawFrame(10, 31, 108, 8);
  int fill = static_cast<int>(progress * 104);
  if (fill > 0) u8g2.drawBox(12, 33, fill, 4);

  char countdown[24];
  snprintf(countdown, sizeof(countdown), "RESET IN %d", seconds_left);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr((W - u8g2.getStrWidth(countdown)) / 2, 46, countdown);
}
