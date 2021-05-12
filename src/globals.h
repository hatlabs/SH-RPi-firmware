#ifndef _globals_H_
#define _globals_H_

#include <Arduino.h>

#include "blinker.h"

//////
// Hardware pin definitions

#define V_IN_ADC_PIN PIN_PA6

#define V_CAP_ADC_PIN PIN_PA7

#define LED_VIN_PIN PIN_PA3
#define LED_VCAP_PIN PIN_PA4
#define LED_STATUS_PIN PIN_PA1

#define EN5V_PIN PIN_PA5

// same as SDA
#define GPIO_POWEROFF_PIN PIN_PB1

//////
// Other behavioral definitions

// if POWEROFF_PIN is low for more than this amount of ms, host is off
#define GPIO_OFF_TIME_LIMIT 1000

// Vcap voltage at which 5V is enabled
#define VCAP_POWER_ON 1.8
// Vcap voltage at which 5V is disabled
#define VCAP_POWER_OFF 1.0
// max scaling value for Vcap voltage
#define VCAP_MAX 2.75
// integer scaling factor for Vcap voltage
#define VCAP_SCALE 1024

// blink the Vin led below this voltage
#define VIN_LOW 11.5
// turn off below this voltage
#define VIN_OFF 10.0
// max voltage for Vin
#define VIN_MAX 32.1
// Vin scaling factor
#define VIN_SCALE 1024

// how long to wait until forcibly shutdown
#define SHUTDOWN_WAIT_DURATION 60000

// how long to stay in off state until restarting
#define OFF_STATE_DURATION 5000

// how long to keep EN5V low in the event of watchdog reboot
#define WATCHDOG_REBOOT_DURATION 2000

//////
// Globals

extern PatternBlinker status_blinker;
extern RatioBlinker led_vin_blinker;
extern RatioBlinker supercap_blinker;

// milliseconds elapsed since last watchdog reset
extern elapsedMillis watchdog_elapsed;
// watchdog time limit
extern int watchdog_limit;
extern bool watchdog_value_changed;

// milliseconds elapsed since gpio-poweroff pin was last high
extern elapsedMillis gpio_poweroff_elapsed;

extern uint8_t i2c_register;

extern unsigned int power_on_vcap_voltage;
extern unsigned int power_off_vcap_voltage;

extern unsigned int v_supercap;
extern unsigned int v_in;

extern bool shutdown_requested;

#endif
