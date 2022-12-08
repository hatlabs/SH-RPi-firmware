#include "shrpi_i2c.h"

#include <Wire.h>

#include "globals.h"
#include "state_machine.h"

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
      Wire.write((FW_VERSION >> 24) & 0xff);
      Wire.write((FW_VERSION >> 16) & 0xff);
      Wire.write((FW_VERSION >> 8) & 0xff);
      Wire.write((FW_VERSION)&0xff);
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
