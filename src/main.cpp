#include <Arduino.h>
#include <Wire.h>
#include <avr/io.h>

#include "analog_io.h"
#include "blinker.h"
#include "digital_io.h"
#include "globals.h"
#include "shrpi_i2c.h"
#include "state_machine.h"

// ATTiny software for monitoring and controlling the Sailor Hat board.

// Spec:

// Act as I2C slave at address 0x6d (or whatever).
// Recognize following commands:
// - Read 0x01: Query hardware version
// - Read 0x02: Query legacy firmware version
// - Read 0x03: Query firmware version
// - Read 0x10: Query Raspi power state
// - Write 0x10 0x00: Set Raspi power off
// - Write 0x10 0x01: Set Raspi power on (who'd ever send that?)
// - Read 0x12: Query watchdog timeout
// - Write 0x12 [NN]: Set watchdog timeout to 0.1*NN seconds
// - Write 0x12 0x00: Disable watchdog
// - Read 0x13: Query power-on threshold voltage
// - Write 0x13 [NN]: Set power-on threshold voltage to 0.01*NN V
// - Read 0x14: Query power-off threshold voltage
// - Write 0x14 [NN]: Set power-off threshold voltage to 0.01*NN V
// - Read 0x15: Query state machine state
// - Read 0x16: Query watchdog elapsed
// - Read 0x20: Query DC IN voltage
// - Read 0x21: Query supercap voltage
// - Read 0x22: Query DC IN current
// - Read 0x23: Query MCU temperature
// - Write 0x30: [ANY]: Initiate shutdown

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
    uint16_t(((uint16_t)-1) * (VCAP_POWER_ON / VCAP_MAX));
LedBlinker led_blinker(led_pins, off_pattern, led_bar_knee_value);

bool shutdown_requested = false;
bool sleep_requested = false;

bool rtc_wakeup_triggered = false;
bool ext_wakeup_triggered = false;

uint8_t i2c_register;

unsigned int power_on_vcap_voltage = int(VCAP_POWER_ON / VCAP_MAX * VCAP_SCALE);
unsigned int power_off_vcap_voltage =
    int(VCAP_POWER_OFF / VCAP_MAX * VCAP_SCALE);

unsigned int v_supercap = 0;
unsigned int v_in = 0;
unsigned int i_in = 0;
uint16_t temperature_K = 0;

// factory calibration of the internal temperature sensor
int8_t sigrow_offset = SIGROW.TEMPSENSE1;
uint8_t sigrow_gain = SIGROW.TEMPSENSE0;

void setup() {
  init_ADC1();
  att1s_analog_reference_adc0(INTERNAL1V1);  // set ADC0 reference to 1.1V
  att1s_analog_reference_adc1(INTERNAL2V5);  // set ADC1 reference to 2.5V

  pinMode(EN5V_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);

  // set up I2C

  // defer the actual BEGIN call until the first step of the state machine
  Wire.onRequest(request_I2C_event);
  Wire.onReceive(receive_I2C_event);

  // set the analog input pins to input
  pinMode(V_CAP_ADC_PIN, INPUT);
  pinMode(V_IN_ADC_PIN, INPUT);
  pinMode(I_IN_ADC_PIN, INPUT);

  // set up digital input GPIO pins to be pulled up
  pinMode(POWER_TOGGLE_PIN, INPUT_PULLUP);
  pinMode(EXT_INT_PIN, INPUT_PULLUP);
  pinMode(RTC_INT_PIN, INPUT_PULLUP);

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
    v_supercap = att1s_analog_read(V_CAP_ADC_AIN, V_CAP_ADC_NUM);
    v_in = att1s_analog_read(V_IN_ADC_AIN, V_IN_ADC_NUM);
    i_in = att1s_analog_read(I_IN_ADC_AIN, I_IN_ADC_NUM);

    unsigned int adc_reading = att1s_analog_read(ADC_TEMPSENSE, 0);
    // temperature compensation code from the datasheet page 435
    uint32_t temp_temp = adc_reading - sigrow_offset;
    temp_temp *= sigrow_gain;
    temp_temp += 0x80;
    temp_temp >>= 1;
    temperature_K = temp_temp;

    // A low value of GPIO_POWEROFF_PIN indicates that the host has shut down
    if (read_pin(GPIO_POWEROFF_PIN) == true) {
      gpio_poweroff_elapsed = 0;
    }

    rtc_wakeup_triggered = !read_pin(RTC_INT_PIN);
    ext_wakeup_triggered = !read_pin(EXT_INT_PIN);

    // if POWER_TOGGLE_PIN is pulled low, initiate shutdown
    if (read_pin(POWER_TOGGLE_PIN) == false) {
      shutdown_requested = true;
    }

    // v_supercap is 10-bit while set_bar input is 16-bit - shift up by 6 bits
    led_blinker.set_bar(v_supercap << 6);
  }

  static elapsedMillis serial_output_elapsed = 0;
  if (serial_output_elapsed > 1000) {
    serial_output_elapsed = 0;

    //Serial.print("0123456789");
    Serial.print("V_sup: ");
    Serial.print(v_supercap);
    
    Serial.print(", V_in: ");
    Serial.print(v_in);
    Serial.print(", I_in: ");
    Serial.print(i_in);
    Serial.print(", temp: ");
    Serial.println(temperature_K);
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

  led_blinker.tick();

  sm_run();
}
