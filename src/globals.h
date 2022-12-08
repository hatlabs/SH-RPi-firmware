#ifndef SH_RPI_FIRMWARE_SRC_GLOBALS_H_
#define SH_RPI_FIRMWARE_SRC_GLOBALS_H_

#include <Arduino.h>

#include "constants.h"
#include "blinker.h"


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

extern LedBlinker led_blinker;

extern bool rtc_wakeup_triggered;
extern bool ext_wakeup_triggered;

extern uint8_t i2c_register;

extern unsigned int power_on_vcap_voltage;
extern unsigned int power_off_vcap_voltage;

extern unsigned int v_supercap;
extern unsigned int v_in;
extern int i_in;
extern uint16_t temperature_K;

extern bool shutdown_requested;
extern bool sleep_requested;

#endif
