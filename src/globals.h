#ifndef _globals_H_
#define _globals_H_

#include <Arduino.h>

#include "blinker.h"

//////
// Hardware pin definitions

// analog input pins

#define V_IN_ADC_PIN PIN_PA6
#define V_CAP_ADC_PIN PIN_PA7
#define I_IN_ADC_PIN PIN_PC3

// digital input pins

#define POWER_TOGGLE_PIN PIN_PC0
#define EXT_INT_PIN PIN_PC1
#define RTC_INT_PIN PIN_PC2

// digital or PWM output pins

#define LED1_PIN PIN_PA2
#define LED2_PIN PIN_PA3
#define LED3_PIN PIN_PA4
#define LED4_PIN PIN_PA5

#define EN5V_PIN PIN_PB4

// same as SDA
#define GPIO_POWEROFF_PIN PIN_PB1

//////
// Other behavioral definitions

// if POWEROFF_PIN is low for more than this amount of ms, host is off
#define GPIO_OFF_TIME_LIMIT 1000

// Vcap voltage at which 5V is enabled
#define VCAP_POWER_ON 6.0
// Vcap voltage at which 5V is disabled
#define VCAP_POWER_OFF 5.0
// max scaling value for Vcap voltage
#define VCAP_MAX 9.35
// integer scaling factor for Vcap voltage
#define VCAP_SCALE 1024

// blink the Vin led below this voltage
#define VIN_LOW 11.5
// turn off below this voltage
#define VIN_OFF 9.0
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
