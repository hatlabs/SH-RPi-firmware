#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <avr/io.h>

#include "analog_io.h"
#include "blinker.h"
#include "digital_io.h"
#include "globals.h"
#include "shrpi_i2c.h"
#include "state_machine.h"

// ATTiny software for monitoring and controlling the Sailor Hat board.

// LEDs:

// STATUS led, slow brief flashes: Watchdog not set
// STATUS led, slow blink: More than 3 s until watchdog timeout
// STATUS led, rapid blink: Watchdog timeout imminent

// VCAP led: indicate supercap charge level

LedPatternSegment off_pattern[] = {
    {{0, 0, 0, 0}, 0b1111, 50},
    {{0, 0, 0, 0}, 0b0000, 0},
};

// define external variables declared in globals.h
volatile bool watchdog_reset = false;
elapsedMillis watchdog_elapsed;
volatile int new_watchdog_limit = -1;
int watchdog_limit = 0;
bool watchdog_value_changed = false;

elapsedMillis gpio_poweroff_elapsed;

int led_pins[] = {LED1_PIN, LED2_PIN, LED3_PIN, LED4_PIN};
constexpr uint16_t led_bar_knee_value =
    uint16_t(((uint16_t)-1) * (LED_BAR_KNEE / VCAP_MAX));
LedBlinker led_blinker(led_pins, off_pattern, led_bar_knee_value);

volatile bool shutdown_requested = false;
volatile bool sleep_requested = false;
volatile bool reset_requested = false;

bool rtc_wakeup_triggered = false;
bool ext_wakeup_triggered = false;

volatile uint8_t i2c_register = 0;

int16_t power_on_vcap_voltage = int(VCAP_POWER_ON / VCAP_MAX * VCAP_SCALE);
int16_t power_off_vcap_voltage = int(VCAP_POWER_OFF / VCAP_MAX * VCAP_SCALE);
int16_t vcap_alarm_voltage = int(VCAP_ALARM / VCAP_MAX * VCAP_SCALE);

bool vcap_alarm_triggered = false;
bool vcap_alarm_changed = false;

int16_t new_power_on_vcap_voltage = -1;
int16_t new_power_off_vcap_voltage = -1;

uint16_t v_supercap = 0;
uint16_t v_in = 0;
uint16_t i_in = 0;
uint16_t temperature_K = 0;

uint8_t led_global_brightness = 0;
uint8_t new_led_global_brightness = 255;

char v_supercap_buf[2];
char v_in_buf[2];
char i_in_buf[2];
char temperature_K_buf[2];

// factory calibration of the internal temperature sensor
int8_t sigrow_offset = SIGROW.TEMPSENSE1;
uint8_t sigrow_gain = SIGROW.TEMPSENSE0;

void setup() {
  init_ADC1();
  att1s_analog_reference_adc0(INTERNAL1V1);  // set ADC0 reference to 1.1V
  att1s_analog_reference_adc1(INTERNAL2V5);  // set ADC1 reference to 2.5V

  pinMode(EN5V_PIN, OUTPUT);

  // set up I2C

  // Use alternate pins for I2C
  Wire.swap(1);

  // defer the actual BEGIN call until the first step of the state machine
  Wire.onReceive(receive_I2C_event);

  // set the analog input pins to input
  pinMode(V_CAP_ADC_PIN, INPUT);
  pinMode(V_IN_ADC_PIN, INPUT);
  pinMode(I_IN_ADC_PIN, INPUT);

  // set up digital input GPIO pins to be pulled up
  pinMode(POWER_TOGGLE_PIN, INPUT_PULLUP);
  pinMode(EXT_INT_PIN, INPUT_PULLUP);
  pinMode(RTC_INT_PIN, INPUT_PULLUP);

  // read the power on voltage from EEPROM
  EEPROM.get(EEPROM_POWER_ON_VCAP_ADDR, power_on_vcap_voltage);
  if (power_on_vcap_voltage < 0 || power_on_vcap_voltage > VCAP_SCALE) {
    power_on_vcap_voltage = int(VCAP_POWER_ON / VCAP_MAX * VCAP_SCALE);
  }

  // read the power off voltage from EEPROM
  EEPROM.get(EEPROM_POWER_OFF_VCAP_ADDR, power_off_vcap_voltage);
  if (power_off_vcap_voltage < 0 || power_off_vcap_voltage > VCAP_SCALE) {
    power_off_vcap_voltage = int(VCAP_POWER_OFF / VCAP_MAX * VCAP_SCALE);
  }

  // Read the LED brightness from EEPROM. The default unset value is 0xFF which
  // just coincides with the default full brightness value.
  EEPROM.get(EEPROM_LED_BRIGHTNESS_ADDR, led_global_brightness);
  new_led_global_brightness = led_global_brightness;

  // setup serial port
  Serial.begin(38400);
  delay(100);
  Serial.println("Starting up...");
}

void loop() {
  static elapsedMillis v_reading_elapsed = 0;

  // no need to read the values at every iteration;
  // 23 ms selected to descynchronize the reading from
  // other loops
  if (v_reading_elapsed > 23) {
    v_reading_elapsed = 0;

    // read the analog inputs twice to let the ADC stabilize

    att1s_analog_read(V_CAP_ADC_AIN, V_CAP_ADC_NUM);
    v_supercap = att1s_analog_read(V_CAP_ADC_AIN, V_CAP_ADC_NUM);
    att1s_analog_read(V_CAP_ADC_AIN, V_CAP_ADC_NUM);
    v_in = att1s_analog_read(V_IN_ADC_AIN, V_IN_ADC_NUM);
    att1s_analog_read(V_CAP_ADC_AIN, V_CAP_ADC_NUM);
    i_in = att1s_analog_read(I_IN_ADC_AIN, I_IN_ADC_NUM);

    if (v_supercap > vcap_alarm_voltage) {
      if (!vcap_alarm_triggered) {
        vcap_alarm_triggered = true;
        vcap_alarm_changed = true;
      }
    } else {
      if (vcap_alarm_triggered) {
        vcap_alarm_triggered = false;
        vcap_alarm_changed = true;
      }
    }

    v_supercap_buf[0] = v_supercap >> 2;
    v_supercap_buf[1] = (v_supercap << 6) & 0xff;

    v_in_buf[0] = v_in >> 2;
    v_in_buf[1] = (v_in << 6) & 0xff;

    i_in_buf[0] = i_in >> 2;
    i_in_buf[1] = (i_in << 6) & 0xff;

    att1s_analog_read(ADC_TEMPSENSE, 0);
    unsigned int adc_reading = att1s_analog_read(ADC_TEMPSENSE, 0);
    // temperature compensation code from the datasheet page 435
    uint32_t temp_temp = adc_reading - sigrow_offset;
    temp_temp *= sigrow_gain;
    temp_temp += 0x80;
    temp_temp >>= 1;  // make temperature fit in uint16_t
    temperature_K = temp_temp;

    temperature_K_buf[0] = temperature_K >> 8;
    temperature_K_buf[1] = temperature_K & 0xff;

    // A low value of GPIO_POWEROFF_PIN indicates that the host has shut down
    if (read_pin(GPIO_POWEROFF_PIN) == true) {
      gpio_poweroff_elapsed = 0;
    }

    rtc_wakeup_triggered = !read_pin(RTC_INT_PIN);

#ifndef HW_VERSION_2_0_0
    ext_wakeup_triggered = !read_pin(EXT_INT_PIN);

    // if POWER_TOGGLE_PIN is pulled low, initiate shutdown
    if (read_pin(POWER_TOGGLE_PIN) == false) {
      shutdown_requested = true;
    }

    // Pulling both EXT_INT_PIN and RTC_INT_PIN low will trigger a reset
    if (ext_wakeup_triggered && shutdown_requested) {
      reset_requested = true;
    }
#endif
    // v_supercap is 10-bit while set_bar input is 16-bit - shift up by 6 bits
    led_blinker.set_bar(v_supercap << 6);
  }

  static elapsedMillis serial_output_elapsed = 0;
  if (serial_output_elapsed > 500) {
    serial_output_elapsed = 0;

    // Serial.print("0123456789");
    Serial.print("State: ");
    Serial.print(get_sm_state_name());
    Serial.print(", V_sup: ");
    Serial.print(v_supercap);
    Serial.print(", V_in: ");
    Serial.print(v_in);
    Serial.print(", I_in: ");
    Serial.print(i_in);
    Serial.print(", temp: ");
    Serial.print(temperature_K);
    Serial.print(", i2c: ");
    Serial.print(i2c_register);
    Serial.print(", RTC: ");
    Serial.print(read_pin(RTC_INT_PIN));
    Serial.print(", EXT: ");
    Serial.print(read_pin(EXT_INT_PIN));
    Serial.print(", PWR: ");
    Serial.print(read_pin(POWER_TOGGLE_PIN));
    Serial.println("");
  }

  if (watchdog_reset) {
    if (new_watchdog_limit != -1) {
      watchdog_value_changed = true;
      watchdog_limit = new_watchdog_limit;
      new_watchdog_limit = -1;
    }

    watchdog_elapsed = 0;
    watchdog_reset = false;
  }

  if (new_power_on_vcap_voltage != -1) {
    power_on_vcap_voltage = new_power_on_vcap_voltage;
    new_power_on_vcap_voltage = -1;
    // write the set value to EEPROM
    EEPROM.put(EEPROM_POWER_ON_VCAP_ADDR, power_on_vcap_voltage);
  }

  if (new_power_off_vcap_voltage != -1) {
    power_off_vcap_voltage = new_power_off_vcap_voltage;
    new_power_off_vcap_voltage = -1;
    // write the set value to EEPROM
    EEPROM.put(EEPROM_POWER_OFF_VCAP_ADDR, power_off_vcap_voltage);
  }

  if (new_led_global_brightness != led_global_brightness) {
    led_global_brightness = new_led_global_brightness;
    // write the set value to EEPROM
    EEPROM.put(EEPROM_LED_BRIGHTNESS_ADDR, led_global_brightness);
  }

  led_blinker.tick();

  sm_run();
}
