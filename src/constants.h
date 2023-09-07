#ifndef SH_RPI_FIRMWARE_SRC_CONSTANTS_H_
#define SH_RPI_FIRMWARE_SRC_CONSTANTS_H_

// FW version provided by the Legacy version I2C register
#define LEGACY_FW_VERSION 0xff

// FW version provided by the new I2C register
constexpr char kFWVersion[] = {2, 0, 5, 0xff};

// HW version provided by the Legacy version I2C register
#define LEGACY_HW_VERSION 0x00

// HW version provided by the new I2C register
constexpr char kHWVersion[] = {2, 0, 0, 0xff};

// Needed for HW bug workarounds for version 2.0.0 only
#define HW_VERSION_2_0_0

// I2C address of the MCU
#define I2C_ADDRESS 0x6d

//////
// Hardware pin definitions

// NOTE: The pin numbers are the Arduino pin numbers, not the AVR pin numbers.
// The Arduino pin numbers do not correspond to the pin numbers on the
// ATtiny1616 VQFN package. Therefore, it is important to use the symbolic
// pin names (e.g. PIN_PA0) instead of the Arduino pin numbers (e.g. 0) in
// the code.

// analog input pins

#define V_CAP_ADC_PIN PIN_PA7
#define V_IN_ADC_PIN PIN_PA6
#define I_IN_ADC_PIN PIN_PC3

// corresponding ADCs and analog input numbers (AIN)

#define V_CAP_ADC_NUM 0
#define V_CAP_ADC_AIN AIN7

#define V_IN_ADC_NUM 0
#define V_IN_ADC_AIN AIN6

#define I_IN_ADC_NUM 1
#define I_IN_ADC_AIN AIN9

// digital input pins

#define POWER_TOGGLE_PIN PIN_PC0
#define EXT_INT_PIN PIN_PC1
#define RTC_INT_PIN PIN_PC2

// digital or PWM output pins

#define NUM_LEDS 4

#define LED1_PIN PIN_PB0
#define LED2_PIN PIN_PB1
#define LED3_PIN PIN_PA3
#define LED4_PIN PIN_PA4

#define EN5V_PIN PIN_PB4

// same as SDA
#define GPIO_POWEROFF_PIN PIN_PA1

//////
// Other behavioral definitions

// if POWEROFF_PIN is low for more than this amount of ms, host is off
#define GPIO_OFF_TIME_LIMIT 1000

// Vcap voltage at which 5V is enabled
#define VCAP_POWER_ON 8.0
// Vcap voltage at which 5V is disabled
#define VCAP_POWER_OFF 5.0
// Vcap voltage should never exceed this value
#define VCAP_ALARM 9.0
// max scaling value for Vcap voltage
#define VCAP_MAX 9.35
// integer scaling factor for Vcap voltage
#define VCAP_SCALE 1024
// Voltage at which the first LED is lit
#define LED_BAR_KNEE 6.0
// maximum value indicated by the LED bar
#define LED_BAR_MAX 9.0

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

// EEPROM addresses
#define EEPROM_POWER_ON_VCAP_ADDR 0   // 2 bytes
#define EEPROM_POWER_OFF_VCAP_ADDR 2  // 2 bytes
#define EEPROM_LED_BRIGHTNESS_ADDR 4  // 1 byte

#endif  // SH_RPI_FIRMWARE_SRC_CONSTANTS_H_