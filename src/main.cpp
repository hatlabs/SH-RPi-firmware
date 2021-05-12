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
// - Read 0x02: Query firmware version
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

// LEDs:

// STATUS led, slow brief flashes: Watchdog not set
// STATUS led, slow blink: More than 3 s until watchdog timeout
// STATUS led, rapid blink: Watchdog timeout imminent

// VCAP led: indicate supercap charge level

#define I2C_ADDRESS 0x6d

#define FW_VERSION 5

int slow_pattern[] = {2050, 2003, -1};

// define external variables declared in globals.h
RatioBlinker led_vin_blinker(LED_VIN_PIN, 200, 0.);
RatioBlinker supercap_blinker(LED_VCAP_PIN, 210, 0.);
PatternBlinker status_blinker(LED_STATUS_PIN, slow_pattern);
volatile bool watchdog_reset = false;
elapsedMillis watchdog_elapsed;
volatile int new_watchdog_limit = -1;
int watchdog_limit = 0;
bool watchdog_value_changed = false;

elapsedMillis gpio_poweroff_elapsed;

bool shutdown_requested = false;

uint8_t i2c_register;

unsigned int power_on_vcap_voltage = int(VCAP_POWER_ON / VCAP_MAX * VCAP_SCALE);
unsigned int power_off_vcap_voltage =
    int(VCAP_POWER_OFF / VCAP_MAX * VCAP_SCALE);

unsigned int v_supercap = 0;
unsigned int v_in = 0;

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
      // Query firmware version
      Wire.write(FW_VERSION);
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
      Wire.write(0x22);
      break;
    default:
      Wire.write(0xff);
  }
}

void setup() {
  analogReference(INTERNAL1V1);  // set analog reference to 1.1V

  pinMode(EN5V_PIN, OUTPUT);
  pinMode(LED_VIN_PIN, OUTPUT);
  pinMode(LED_VCAP_PIN, OUTPUT);
  pinMode(LED_STATUS_PIN, OUTPUT);

  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(request_I2C_event);
  Wire.onReceive(receive_I2C_event);
}

void loop() {
  static elapsedMillis v_reading_elapsed;
  // no need to read the values at every iteration;
  // 47 ms selected to descynchronize the reading from
  // other loops
  if (v_reading_elapsed > 47) {
    v_reading_elapsed = 0;
    // read twice to ensure the ADC output is stable
    analogRead(V_CAP_ADC_PIN);
    v_supercap = analogRead(V_CAP_ADC_PIN);
    analogRead(V_IN_ADC_PIN);
    v_in = analogRead(V_IN_ADC_PIN);
    if (read_pin(GPIO_POWEROFF_PIN) == true) {
      gpio_poweroff_elapsed = 0;
    }

    // v_supercap and v_dcin have 10 bit range, 0..1023
    // ratio has 15 bit range, 0..32767

    supercap_blinker.set_ratio(v_supercap *
                               int(BLINKER_PERIOD_SCALE / VCAP_SCALE));

    if (v_in > int(VIN_LOW / VIN_MAX * VIN_SCALE)) {
      // LED always on
      led_vin_blinker.set_ratio(BLINKER_PERIOD_SCALE);
    } else if (v_in > int(VIN_OFF / VIN_MAX * VIN_SCALE)) {
      // LED 50% on
      led_vin_blinker.set_ratio(int(BLINKER_PERIOD_SCALE / 2));
    } else {
      // LED off
      led_vin_blinker.set_ratio(0);
    }
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

  led_vin_blinker.tick();
  supercap_blinker.tick();
  status_blinker.tick();

  sm_run();
}
