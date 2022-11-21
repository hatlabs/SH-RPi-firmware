#ifndef _globals_H_
#define _globals_H_

#include <Arduino.h>

#include "constants.h"
#include "blinker.h"


//////
// Globals

// milliseconds elapsed since last watchdog reset
extern elapsedMillis watchdog_elapsed;
// watchdog time limit
extern int watchdog_limit;
extern bool watchdog_value_changed;

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

extern bool shutdown_requested;
extern bool sleep_requested;

#endif
