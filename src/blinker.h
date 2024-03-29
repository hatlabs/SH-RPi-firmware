#ifndef _blinker_H_
#define _blinker_H_

#include <elapsedMillis.h>

#include "cie1931.h"
#include "constants.h"
#include "digital_io.h"

#define BLINKER_PERIOD_SCALE 32768

#ifndef MILLIS_USE_TIMERD0
#error "This sketch is written for use with TCD0 as the millis timing source"
#endif

// kludgy forward declaration
extern uint8_t led_global_brightness;

/**
 * @brief Define an individual LED pattern segment.
 *
 * A blink pattern is an array of LedPatternSegments. The final (sentinel)
 * element of the array must have a duration of 0. Once that is reached,
 * the pattern will loop back to the beginning.
 *
 * For ease of use, the mask bit array has reversed order. The first bit
 * in the array corresponds to the last LED in the pattern. Thus, a brightness
 * array of {0, 0, 255, 0} and a mask of 0b0010 will both refer to the third
 * LED in the pattern.
 */
struct LedPatternSegment {
  uint8_t brightness[NUM_LEDS];  //!< Brightness of each LED in the segment
  uint8_t mask;                  //!< Mask of LEDs to apply the segment to
  uint16_t duration;             //!< Duration of the segment in milliseconds
};

/**
 * @brief LED array blinker class.
 *
 * This class is used to blink the LED array. It is used to indicate both
 * the current charge level of the supercap and the current state of the
 * device.
 *
 * Operation logic is as follows: All of the LEDs are operated in a PWM
 * mode. The brightness of each LED is controlled by a 8-bit value. The
 * Supercap voltage level is presented as a bar display. Overlaid on top
 * of the bar display is a pattern defined by LedPatternSegments. The pattern
 * can be used to blink or animate the LEDs on top of the bar display.
 *
 * The bar display is non-linear. The first LED is fully lit when the
 * bar value equals to bar_knee_value. All LEDs are fully lit when the
 * bar value equals to bar_max_value.
 */
class LedBlinker {
 public:
  LedBlinker(int* pins, LedPatternSegment* pattern, uint16_t bar_knee_value)
      : pattern_{pattern}, bar_knee_value_{bar_knee_value} {
    for (int i = 0; i < NUM_LEDS; i++) {
      pin_[i] = pins[i];
      port_[i] = digitalPinToPortStruct(pins[i]);
      pin_mask_[i] = digitalPinToBitMask(pins[i]);
      pinMode(pins[i], OUTPUT);
    }
    value_step = (bar_max_value_ - bar_knee_value_) / (NUM_LEDS - 1);
  }

  void set_pattern(LedPatternSegment* pattern) {
    // only set if the new pattern is different from the current pattern
    if (pattern == pattern_) {
      return;
    }
    pattern_ = pattern;
    pattern_index_ = 0;
    pattern_timer_ = 0;
    update_led_values();
  }

  void set_bar(uint16_t value) {
    if (value < bar_knee_value_) {
      // the first LED brightness is proportional to 0..bar_knee_value_ V
      bar_value_[0] = (255L * value ) / bar_knee_value_;
      for (int i = 1; i < NUM_LEDS; i++) {
        bar_value_[i] = 0;
      }
    } else if (value >= bar_max_value_) {
      // all LEDs are fully lit
      for (int i = 1; i < NUM_LEDS; i++) {
        bar_value_[i] = 255;
      }
    } else {
      // Value is between bar_knee_value_ and bar_max_value_. The first LED
      // is fully lit and the remaining LEDs are lit proportionally to the
      // voltage value.

      // set the first LED to full brightness
      bar_value_[0] = 255;

      // determine how many additional LEDs are fully lit
      int num_full_leds = (value - bar_knee_value_) / value_step;
      // set all lit leds
      for (int i = 1; i < num_full_leds + 1; i++) {
        bar_value_[i] = 255;
      }
      // set all unlit leds
      for (int i = num_full_leds + 2; i < NUM_LEDS; i++) {
        bar_value_[i] = 0;
      }
      // determine the value of the partially lit LED
      uint32_t new_value =
          255 *
          ((uint32_t)value - bar_knee_value_ - num_full_leds * value_step) /
          value_step;
      bar_value_[num_full_leds + 1] = new_value;
    }
    update_led_values();
  }

  void init() {
    // Pin modes have been set in the main program.
    // Set the PWM registers here.
  }

  void tick() {
    if (pattern_timer_ > pattern_[pattern_index_].duration) {
      pattern_index_++;
      if (pattern_[pattern_index_].duration == 0) {
        pattern_index_ = 0;
      }
      update_led_values();
      pattern_timer_ = 0;
    }
  }

  uint8_t bar_value_[NUM_LEDS];  //!< Bar display brightness value for each LED

 protected:
  PORT_t* port_[NUM_LEDS];      //!< Port for each LED
  uint8_t pin_[NUM_LEDS];       //!< Pin for each LED
  uint8_t pin_mask_[NUM_LEDS];  //!< Port mask for each LED pin
  uint8_t
      led_value_[NUM_LEDS];     //!< Current final brightness value for each LED
  LedPatternSegment* pattern_;  //!< Pointer to the current pattern
  uint8_t pattern_index_ = 0;   //!< Index of the current pattern segment
  elapsedMillis pattern_timer_ = 0;  //!< Timer for the current pattern segment
  uint16_t bar_knee_value_;          //!< Knee value for the bar display
  static constexpr uint16_t bar_max_value_ = uint16_t(
      ((uint16_t)-1) * 9.0 / VCAP_MAX);  //!< Maximum value for the bar display
  uint16_t value_step;  //!< Value increase between each LED in the bar display

  /**
   * @brief Update the LED output values.
   *
   */
  void update_led_values() {
    // get the current pattern segment mask
    uint8_t mask = pattern_[pattern_index_].mask;
    for (int i = 0; i < NUM_LEDS; i++) {
      if (mask & (1 << NUM_LEDS - 1 - i)) {
        uint16_t new_brightness =
            led_global_brightness * pattern_[pattern_index_].brightness[i];
        led_value_[i] = new_brightness >> 8;
      } else {
        uint16_t new_brightness = led_global_brightness * bar_value_[i];
        led_value_[i] = new_brightness >> 8;
      }
      // Use a lookup table to map the logarithmic sensitivity of the human
      // eye to the linear PWM output. Or the other way around?
      analogWrite(pin_[i], cie[led_value_[i]]);
    }
  }
};

#endif
