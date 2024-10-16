#ifndef SH_RPI_FIRMWARE_SRC_GLOBALS_H_
#define SH_RPI_FIRMWARE_SRC_GLOBALS_H_

#include <Arduino.h>

#include "blinker.h"
#include "constants.h"

//////
// Globals

// milliseconds elapsed since last watchdog reset
extern elapsedMillis watchdog_elapsed;
// watchdog time limit
extern int watchdog_limit;
// new watchdog time limit set by the I2C event handler
extern volatile int new_watchdog_limit;
extern bool watchdog_value_changed;
// set true whenever an i2c call is made
extern volatile bool watchdog_reset;

// milliseconds elapsed since gpio-poweroff pin was last high
extern elapsedMillis gpio_poweroff_elapsed;

extern bool rtc_wakeup_triggered;
extern bool ext_wakeup_triggered;

extern volatile uint8_t i2c_register;

extern int16_t power_on_vcap_voltage;
extern int16_t power_off_vcap_voltage;
extern int16_t vcap_alarm_voltage;
extern bool vcap_alarm_triggered;
extern bool vcap_alarm_changed;

extern int16_t new_power_on_vcap_voltage;
extern int16_t new_power_off_vcap_voltage;

extern uint8_t led_global_brightness;
extern uint8_t new_led_global_brightness;

extern uint16_t v_supercap;
extern uint16_t v_in;
extern uint16_t i_in;
extern uint16_t temperature_K;

extern char v_supercap_buf[2];
extern char v_in_buf[2];
extern char i_in_buf[2];
extern char temperature_K_buf[2];

extern volatile bool shutdown_requested;
extern volatile bool sleep_requested;
extern volatile bool reset_requested;

extern LedBlinker led_blinker;

#endif
