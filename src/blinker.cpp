#include "blinker.h"

#include "digital_io.h"

BaseBlinker::BaseBlinker(int pin) : pin_{pin} {
  port_ = digitalPinToPortStruct(pin);
  pin_mask_ = digitalPinToBitMask(pin);
}

PeriodicBlinker::PeriodicBlinker(int pin, unsigned int period)
    : BaseBlinker(pin), period{period} {}

EvenBlinker::EvenBlinker(int pin, unsigned int period)
    : PeriodicBlinker(pin, period) {}

void EvenBlinker::tick() {
  if (elapsed > period) {
    elapsed = 0;
    state_ = !state_;
    update_pin(port_, pin_mask_, state_);
  }
}

RatioBlinker::RatioBlinker(int pin, unsigned int period,
                           unsigned int ratio)
    : PeriodicBlinker(pin, period), ratio{ratio} {}

void RatioBlinker::tick() {
  int on_duration = (long)ratio * period / BLINKER_PERIOD_SCALE;
  int off_duration = max(0, period - on_duration);
  unsigned int ref_duration = state_ == false ? off_duration : on_duration;
  if (elapsed > ref_duration) {
    elapsed = 0;
    state_ = !state_;
    update_pin(port_, pin_mask_, state_);
  }
}

PatternBlinker::PatternBlinker(int pin, int pattern[])
    : BaseBlinker(pin), pattern{pattern} {}

void PatternBlinker::set_pattern(int pattern[]) {
  this->pattern = pattern;
  state_ = false;
  pattern_ptr = 0;
}

void PatternBlinker::tick() {
  if (elapsed > pattern[pattern_ptr]) {
    elapsed = 0;
    pattern_ptr++;
    // loop to zero if we reached the end
    if (pattern[pattern_ptr] == -1) {
      pattern_ptr = 0;
      state_ = false;
    }
    state_ = !state_;

    update_pin(port_, pin_mask_, state_);
  }
}

void PatternBlinker::restart() {
  state_ = false;
  pattern_ptr = 0;
  elapsed = 0;
}
