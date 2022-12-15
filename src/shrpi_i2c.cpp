#include "shrpi_i2c.h"

#include <Wire.h>

#include "globals.h"
#include "state_machine.h"

// Spec:

// Act as I2C slave at address 0x6d (or whatever).
// Recognize following commands:
// - Read 0x01: Query hardware version
// - Read 0x02: Query legacy firmware version
// - Read 0x03: Query hardware version
// - Read 0x04: Query firmware version
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
// - Write 0x31: [ANY]: Initiate sleep shutdown

void request_I2C_event_0x01() {
  // Query hardware version
  Wire.write(0xff);
}

void request_I2C_event_0x02() {
  // Query legacy firmware version
  Wire.write(LEGACY_FW_VERSION);
}

void request_I2C_event_0x03() {
  // Query hardware version
  Wire.write(kHWVersion[0]);
  Wire.write(kHWVersion[1]);
  Wire.write(kHWVersion[2]);
  Wire.write(kHWVersion[3]);
  // Wire.write(kHWVersion, 4);
}

void request_I2C_event_0x04() {
  // Query firmware version
  Wire.write(kFWVersion[0]);
  Wire.write(kFWVersion[1]);
  Wire.write(kFWVersion[2]);
  Wire.write(kFWVersion[3]);
  // Wire.write(kFWVersion, 4);
}

void request_I2C_event_0x10() {
  // Query 5V power state
  Wire.write(read_pin(EN5V_PIN));
}

void request_I2C_event_0x12() {
  // Query watchdog time remaining
  // FIXME: magic numbers
  Wire.write(watchdog_limit >> 8);
  Wire.write(watchdog_limit & 0xff);
}

void request_I2C_event_0x13() {
  // Query power-on threshold voltage
  // power_on_vcap_voltage is stored as a 10-bit value, but
  // it should be scaled and transmitted as a 16-bit word.
  Wire.write(char(power_on_vcap_voltage >> 2));
  Wire.write(char((power_on_vcap_voltage << 6) & 0xff));
}

void request_I2C_event_0x14() {
  // Query power-off threshold voltage
  // power_off_vcap_voltage is stored as a 10-bit value, but
  // it should be scaled and transmitted as a 16-bit word.
  Wire.write(char(power_off_vcap_voltage >> 2));
  Wire.write(char((power_off_vcap_voltage << 6) & 0xff));
}

void request_I2C_event_0x15() {
  // Query state-machine state
  Wire.write(get_sm_state());
}

void request_I2C_event_0x16() {
  // Query watchdog elapsed
  // FIXME: magic numbers
  Wire.write(watchdog_elapsed / 100);
}

void request_I2C_event_0x20() {
  // Query DC IN voltage
  Wire.write(v_in_buf, 2);
}

void request_I2C_event_0x21() {
  // Query supercap voltage
  Wire.write(v_supercap_buf, 2);
}

void request_I2C_event_0x22() {
  // Query DC IN current
  Wire.write(i_in_buf, 2);
}

void request_I2C_event_0x23() {
  // Query MCU temperature
  Wire.write(temperature_K_buf, 2);
}

void request_I2C_event_unknown() {
  // Ignore other registers
  Wire.write(0);
}

void (*request_I2C_event_array_0x0[])() = {
    request_I2C_event_unknown, request_I2C_event_0x01, request_I2C_event_0x02,
    request_I2C_event_0x03,    request_I2C_event_0x04,
};

void (*request_I2C_event_array_0x1[])() = {
    request_I2C_event_0x10, request_I2C_event_unknown, request_I2C_event_0x12,
    request_I2C_event_0x13, request_I2C_event_0x14,    request_I2C_event_0x15,
    request_I2C_event_0x16,
};

void (*request_I2C_event_array_0x2[])() = {
    request_I2C_event_0x20,
    request_I2C_event_0x21,
    request_I2C_event_0x22,
    request_I2C_event_0x23,
};

void receive_I2C_event(int bytes) {
  // watchdog is considered zeroed after any input
  watchdog_reset = true;

  if (bytes == 1) {
    // We can assume this is a register read request
    i2c_register = Wire.read();
    char register_group = (i2c_register >> 4);
    char register_index = i2c_register & 0x0f;
    switch (register_group) {
      case 0x00:
        // check that register_index is within array bounds
        if (register_index > 4) {
          Wire.onRequest(request_I2C_event_unknown);
          return;
        } else {
          Wire.onRequest(request_I2C_event_array_0x0[register_index]);
          return;
        }
        break;
      case 0x1:
        if (register_index > 6) {
          Wire.onRequest(request_I2C_event_unknown);
          return;
        } else {
          Wire.onRequest(request_I2C_event_array_0x1[register_index]);
          return;
        }
        break;
      case 0x2:
        if (register_index > 3) {
          Wire.onRequest(request_I2C_event_unknown);
          return;
        } else {
          Wire.onRequest(request_I2C_event_array_0x2[register_index]);
          return;
        }
        break;
      default:
        Wire.onRequest(request_I2C_event_unknown);
        return;
        break;
    }
  }

  // If there are more than 1 byte, then the master is writing to the slave.
  // We can go through a long switch statement because the transmission
  // has been already received in the RX buffer.
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
      new_watchdog_limit = Wire.read() << 8 | Wire.read();
      break;
    case 0x13:
      // Set power-on threshold voltage
      power_on_vcap_voltage = (Wire.read() << 2) | (Wire.read() >> 6);
      break;
    case 0x14:
      // Set power-off threshold voltage
      power_off_vcap_voltage = (Wire.read() << 2) | (Wire.read() >> 6);
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
