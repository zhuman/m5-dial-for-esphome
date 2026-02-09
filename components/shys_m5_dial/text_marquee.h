#pragma once
#include <string>
#include <cmath>
#include <M5GFX.h>

namespace esphome {
namespace shys_m5_dial {

class TextMarquee {
 public:
  void setText(const std::string &text) {
    if (text_ == text) return;
    text_ = text;
    text_width_ = -1;
    start_ms_ = 0;
    last_offset_ = -1;
  }

  void setViewport(int16_t width) {
    if (viewport_width_ == width) return;
    viewport_width_ = width;
    text_width_ = -1;
    start_ms_ = 0;
    last_offset_ = -1;
  }

  void setDuration(uint32_t ms) { duration_ms_ = ms; }

  // Returns true if offset changed and needs redraw
  bool update(LovyanGFX *gfx, uint32_t now_ms) {
    if (!gfx || viewport_width_ <= 0) return false;
    if (text_width_ < 0) {
      text_width_ = gfx->textWidth(text_.c_str());
      max_offset_ = std::max<int16_t>(0, text_width_ - viewport_width_);
      if (max_offset_ <= 0) {
        last_offset_ = offset_ = 0;
        return false;
      }
      start_ms_ = now_ms;
    }
    if (max_offset_ <= 0) { offset_ = 0; return last_offset_ != offset_; }

    uint32_t cycle = duration_ms_ * 2;
    uint32_t t_ms = (cycle == 0) ? 0 : (now_ms - start_ms_) % cycle;
    float t = (float)t_ms / (float)duration_ms_;
    if (t > 1.0f) t = 2.0f - t;
    // ease in/out: cosine
    float ease = 0.5f - 0.5f * std::cosf(M_PI * t);
    offset_ = (int16_t)std::round(max_offset_ * ease);
    bool changed = (offset_ != last_offset_);
    last_offset_ = offset_;
    return changed;
  }

  void draw(LovyanGFX *gfx, int16_t cx, int16_t y, uint16_t color) {
    if (!gfx) return;
    gfx->setTextColor(color);
    gfx->setTextDatum(textdatum_t::middle_center);
    gfx->drawString(text_.c_str(), cx - offset_ + max_offset_ / 2, y);
  }

  bool isActive() const { return max_offset_ > 0; }

 private:
  std::string text_;
  int16_t viewport_width_ = 0;
  int16_t text_width_ = -1;
  int16_t max_offset_ = 0;
  int16_t offset_ = 0;
  int16_t last_offset_ = -1;
  uint32_t duration_ms_ = 2000; // full left->right cycle is 2x
  uint32_t start_ms_ = 0;
};

}  // namespace shys_m5_dial
}  // namespace esphome
