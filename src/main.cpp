#include <Arduino.h>
#include <Wire.h>
#include <avr/io.h>

#include "blinker.h"
#include "digital_io.h"
#include "globals.h"
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

#define I2C_ADDRESS 0x6d

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
constexpr uint16_t led_bar_knee_value = uint16_t(((uint16_t)-1) * (VCAP_POWER_ON/VCAP_MAX));
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
int i_in = 0;
uint16_t temperature_K = 0;

// factory calibration of the internal temperature sensor
int8_t sigrow_offset = SIGROW.TEMPSENSE1;
uint8_t sigrow_gain = SIGROW.TEMPSENSE0;


void receive_I2C_event(int bytes) {
  // watchdog is considered zeroed after any input
  watchdog_reset = true;

  // Read the first byte to determine which register is concerned
  i2c_register = Wire.read();

  if (bytes > 1) {
    // If there are more than 1 byte, then the master is writing to the slave
    switch (i2c_register) {
      case 0x10:
        // Set 5V power state
        // FIXME: this should change the state machine state
        set_en5v_pin(Wire.read());
        break;
      case 0x11:
        // This used to set ENIN (input voltage to Vcap) state.
        // However, hardware no longer has support for controlling it,
        // so this is just ignored.
        Wire.read();
        break;
      case 0x12:
        // Set or disable watchdog timer
        // FIXME: magic numbers
        new_watchdog_limit = 100 * Wire.read();
        break;
      case 0x13:
        // Set power-on threshold voltage
        // FIXME: magic numbers
        power_on_vcap_voltage = 4 * Wire.read();
        break;
      case 0x14:
        // Set power-off threshold voltage
        // FIXME: magic numbers
        power_off_vcap_voltage = 4 * Wire.read();
        break;
      case 0x30:
        // Set shutdown initiated
        Wire.read();
        shutdown_requested = true;
        break;
      case 0x31:
        // Set sleep initiated
        Wire.read();
        sleep_requested = true;
        break;
      default:
        // Ignore other registers
        Wire.read();
        break;
    }
  }

  // Consume any extra input
  int available = Wire.available();
  if (available) {
    for (int i = 0; i < available; i++) {
      Wire.read();
    }
  }
}

void request_I2C_event() {
  switch (i2c_register) {
    case 0x01:
      // Query hardware version
      // NB: Feature removed!
      Wire.write(0);
      break;
    case 0x02:
      // Query legacy firmware version
      Wire.write(LEGACY_FW_VERSION);
      break;
    case 0x03:
      // Query firmware version
      Wire.write((FW_VERSION>>24) & 0xff);
      Wire.write((FW_VERSION>>16) & 0xff);
      Wire.write((FW_VERSION>>8) & 0xff);
      Wire.write((FW_VERSION) & 0xff);
      break;
    case 0x10:
      // Query 5V power state
      // FIXME: should this be done in the main loop?
      Wire.write(digitalRead(EN5V_PIN));
      break;
    case 0x11:
      // This used to query ENIN state, however, the functionality
      // has been removed from the hardware.
      Wire.write(1);
      break;
    case 0x12:
      // Query watchdog time remaining
      // FIXME: magic numbers
      Wire.write(watchdog_limit / 100);
      break;
    case 0x13:
      // Query power-on threshold voltage
      // FIXME: magic numbers
      Wire.write(uint8_t(power_on_vcap_voltage / 4));
      break;
    case 0x14:
      // Query power-off threshold voltage
      // FIXME: magic numbers
      Wire.write(uint8_t(power_off_vcap_voltage / 4));
      break;
    case 0x15:
      // Query state machine state
      Wire.write(get_sm_state());
      break;
    case 0x16:
      // Query watchdog elapsed
      // FIXME: magic numbers
      Wire.write(watchdog_elapsed / 100);
      break;
    case 0x20:
      // Query DC IN voltage
      // FIXME: magic numbers
      Wire.write(uint8_t(v_in / 4));
      break;
    case 0x21:
      // Query supercap voltage
      // FIXME: magic numbers
      Wire.write(uint8_t(v_supercap / 4));
      break;
    case 0x22:
      // Query DC IN current
      Wire.write(uint8_t(i_in / 4));
      break;
    case 0x23:
      // Query MCU temperature
      Wire.write(uint8_t(temperature_K >> 8));
      Wire.write(uint8_t(temperature_K & 0xff));
      break;
    default:
      Wire.write(0xff);
  }
}

void setup() {
  analogReference(INTERNAL1V1);  // set analog reference to 1.1V

  pinMode(EN5V_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);

  // set up I2C

  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(request_I2C_event);
  Wire.onReceive(receive_I2C_event);

  // setup serial port
  Serial.begin(38400);
  delay(100);
  Serial.println("Starting up...");
}

void loop() {
  static elapsedMillis serial_output_elapsed = 0;
  static elapsedMillis v_reading_elapsed = 0;

  // no need to read the values at every iteration;
  // 23 ms selected to descynchronize the reading from
  // other loops
  if (v_reading_elapsed > 23) {
    v_reading_elapsed = 0;
    // read twice to ensure the ADC output is stable
    analogRead(V_CAP_ADC_PIN);
    v_supercap = analogRead(V_CAP_ADC_PIN);
    analogRead(V_IN_ADC_PIN);
    v_in = analogRead(V_IN_ADC_PIN);
    analogRead(I_IN_ADC_PIN);
    i_in = analogRead(I_IN_ADC_PIN);
    if (i_in < 0) {
      i_in = 0;
    }

    analogRead(ADC_TEMPERATURE);
    int adc_reading = analogRead(ADC_TEMPERATURE);
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

  if (serial_output_elapsed > 1000) {
    serial_output_elapsed = 0;

    //Serial.print("0123456789");
    Serial.print("V_sup: ");
    Serial.println(v_supercap);
    
    Serial.print(" V_in: ");
    Serial.println(v_in);
    Serial.print("I_in: ");
    Serial.println(i_in);
    Serial.print("gpio_poweroff_elapsed: ");
    Serial.println(gpio_poweroff_elapsed);
    Serial.print("rtc_wakeup_triggered: ");
    Serial.println(rtc_wakeup_triggered);
    Serial.print("ext_wakeup_triggered: ");
    Serial.println(ext_wakeup_triggered);
    Serial.print("temperature: ");
    Serial.println(temperature_K);
    Serial.println();
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
