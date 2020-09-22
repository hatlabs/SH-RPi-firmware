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
// - Read 0x20: Query DC IN (12V) voltage
// - Read 0x21: Query supercap voltage

// LEDs:

// STA2 led, slow brief flashes: Watchdog not set
// STA2 led, slow blink: More than 3 s until watchdog timeout
// STA2 led, rapid blink: Watchdog timeout imminent

// STA1 led: indicate supercap charge level

#define I2C_ADDRESS 0x6d

#define FW_VERSION 4

int slow_pattern[] = {2050, 2003, -1};

// define external variables declared in globals.h
RatioBlinker led_12v_blinker(LED12V_PORT, LED12V_PIN, 200, 0.);
RatioBlinker supercap_blinker(LEDSTA1_PORT, LEDSTA1_PIN, 210, 0.);
PatternBlinker status_blinker(LEDSTA2_PORT, LEDSTA2_PIN, slow_pattern);
elapsedMillis watchdog_elapsed;
volatile int watchdog_limit = 0;
bool watchdog_value_changed = false;

bool shutdown_requested = false;

uint8_t i2c_register;

// scaling factor for these: 2.8V eq 1023
unsigned int power_on_vcap_voltage = 548;   // 1.5V/2.8V * 1023
unsigned int power_off_vcap_voltage = 365;  // 1.0V/2.8V * 1023

// scaling factor: 2.8V eq 1023
unsigned int v_supercap = 0;
// scaling factor: 32V eq 1023
unsigned int v_dcin = 0;

void receive_I2C_event(int bytes) {
  // watchdog is considered zeroed after any input
  watchdog_elapsed = 0;

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
        watchdog_limit = 100 * Wire.read();
        watchdog_value_changed = true;
        watchdog_elapsed = 0;
        break;
      case 0x13:
        // Set power-on threshold voltage
        power_on_vcap_voltage = 4 * Wire.read();
        break;
      case 0x14:
        // Set power-off threshold voltage
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

uint8_t get_hw_version() {
  int voltage = analogRead(VERSION_PIN);
  if (voltage < 5) {  // 5 / 1023 * 1.1 V = 0.0054 V
    return 7;
  } else if (voltage > 874) {  // 874 / 1023 * 1.1 V = 0.94 V
    return 8;
  } else {
    return 0;
  }
}

void request_I2C_event() {
  switch (i2c_register) {
    case 0x01:
      // Query hardware version
      Wire.write(get_hw_version());
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
      Wire.write(watchdog_limit / 100);
      break;
    case 0x13:
      // Query power-on threshold voltage
      Wire.write(uint8_t(power_on_vcap_voltage / 4));
      break;
    case 0x14:
      // Query power-off threshold voltage
      Wire.write(uint8_t(power_off_vcap_voltage / 4));
      break;
    case 0x15:
      // Query state machine state
      Wire.write(get_sm_state());
      break;
    case 0x16:
      // Query watchdog elapsed
      Wire.write(watchdog_elapsed / 100);
      break;
    case 0x20:
      // Query DC IN voltage
      Wire.write(uint8_t(v_dcin / 4));
      break;
    case 0x21:
      // Query supercap voltage
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

  // ensure that watchdog is disabled
  watchdog_limit = 0;

  set_port_mode(&port_a_mode, &port_b_mode, EN5V_PORT, EN5V_PIN, OUTPUT);
  set_port_mode(&port_a_mode, &port_b_mode, LED12V_PORT, LED12V_PIN, OUTPUT);
  set_port_mode(&port_a_mode, &port_b_mode, LEDSTA1_PORT, LEDSTA1_PIN, OUTPUT);
  set_port_mode(&port_a_mode, &port_b_mode, LEDSTA2_PORT, LEDSTA2_PIN, OUTPUT);

  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(request_I2C_event);
  Wire.onReceive(receive_I2C_event);
}

void loop() {
  static elapsedMillis v_reading_elapsed;
  if (v_reading_elapsed > 50) {
    v_reading_elapsed = 0;
    v_supercap = analogRead(V_CAP_ADC_PIN);
    v_dcin = analogRead(V_DCIN_ADC_PIN);

    // v_supercap and v_dcin have 10 bit range, 0..1023
    // ratio has 15 bit range, 0..32768

    supercap_blinker.set_ratio(v_supercap * 32);
    led_12v_blinker.set_ratio(v_dcin * 32);
  }

  led_12v_blinker.tick(&port_a_state, &port_b_state);
  supercap_blinker.tick(&port_a_state, &port_b_state);
  status_blinker.tick(&port_a_state, &port_b_state);

  // update ports based on the blinker updates

  DDRA = port_a_mode;
  PORTA = port_a_state;
  DDRB = port_b_mode;
  PORTB = port_b_state;

  sm_run();

  // update port A based on the state machine updates (EN5V may change)

  DDRA = port_a_mode;
  PORTA = port_a_state;
}
