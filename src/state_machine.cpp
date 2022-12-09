#include "state_machine.h"

#include <Wire.h>

#include "digital_io.h"
#include "globals.h"

// take care to have all enum values of StateType present
void (*state_machine[])(void) = {sm_state_BEGIN,
                                 sm_state_WAIT_VIN_ON,
                                 sm_state_ENT_CHARGING,
                                 sm_state_CHARGING,
                                 sm_state_ENT_ON,
                                 sm_state_ON,
                                 sm_state_ENT_DEPLETING,
                                 sm_state_DEPLETING,
                                 sm_state_ENT_SHUTDOWN,
                                 sm_state_SHUTDOWN,
                                 sm_state_ENT_WATCHDOG_REBOOT,
                                 sm_state_WATCHDOG_REBOOT,
                                 sm_state_ENT_OFF,
                                 sm_state_OFF,
                                 sm_state_ENT_SLEEP_SHUTDOWN,
                                 sm_state_SLEEP_SHUTDOWN,
                                 sm_state_ENT_SLEEP,
                                 sm_state_SLEEP};

char *state_names[] = {
    "BEGIN",        "WAIT_VIN_ON", "ENT_CHARGING",        "CHARGING",
    "ENT_ON",       "ON",          "ENT_DEPLETING",       "DEPLETING",
    "ENT_SHUTDOWN", "SHUTDOWN",    "ENT_WATCHDOG_REBOOT", "WATCHDOG_REBOOT",
    "ENT_OFF",      "OFF",         "ENT_SLEEP_SHUTDOWN",  "SLEEP_SHUTDOWN",
    "ENT_SLEEP",    "SLEEP",
};

StateType sm_state = BEGIN;

StateType get_sm_state() { return sm_state; }

// briefly blink all leds
LedPatternSegment power_off_pattern[] = {
    {{255, 255, 255, 255}, 0b0000, 50},
    {{0, 0, 0, 0}, 0b1111, 3900},
    {{0, 0, 0, 0}, 0b0000, 0},
};

void sm_state_BEGIN() {
  set_en5v_pin(false);
  Wire.begin(I2C_ADDRESS);

  i2c_register = 0xff;
  watchdog_limit = 0;
  gpio_poweroff_elapsed = 0;

  led_blinker.set_pattern(power_off_pattern);

  sm_state = WAIT_VIN_ON;
}

void sm_state_WAIT_VIN_ON() {
  // never start if DC input voltage is not present
  if (v_in >= int(VIN_OFF / VIN_MAX * VIN_SCALE)) {
    sm_state = ENT_CHARGING;
  }
}

// just show the underlying bar display
LedPatternSegment no_pattern[] = {
    {{0, 0, 0, 0}, 0b0000, 100},
    {{0, 0, 0, 0}, 0b0000, 0},
};

void sm_state_ENT_CHARGING() {
  led_blinker.set_pattern(no_pattern);
  sm_state = CHARGING;
}

void sm_state_CHARGING() {
  if (v_supercap > power_on_vcap_voltage) {
    sm_state = ENT_ON;
  } else if (v_in < int(VIN_OFF / VIN_MAX * VIN_SCALE)) {
    // if power is cut before supercap is charged,
    // kill power immediately
    sm_state = ENT_OFF;
  }
}

void sm_state_ENT_ON() {
  set_en5v_pin(true);
  led_blinker.set_pattern(no_pattern);
  gpio_poweroff_elapsed = 0;
  sm_state = ON;
}

LedPatternSegment watchdog_pattern[] = {
    {{255, 255, 255, 255}, 0b0000, 3950},
    {{0, 0, 0, 0}, 0b1111, 50},
    {{0, 0, 0, 0}, 0b0000, 0},
};

void sm_state_ON() {
  if (watchdog_value_changed) {
    if (watchdog_limit) {
      Serial.println("Watchdog enabled");
      led_blinker.set_pattern(watchdog_pattern);
    } else {
      Serial.println("Watchdog disabled");
      led_blinker.set_pattern(no_pattern);
    }
    watchdog_value_changed = false;
  }
  if (watchdog_limit && (watchdog_elapsed > watchdog_limit)) {
    sm_state = ENT_WATCHDOG_REBOOT;
    return;
  }

  if (shutdown_requested) {
    shutdown_requested = false;
    sm_state = ENT_SHUTDOWN;
    return;
  }

  if (sleep_requested) {
    sm_state = ENT_SLEEP_SHUTDOWN;
    return;
  }

  // kill the power if the host has been powered off for more than a second
  if (gpio_poweroff_elapsed > GPIO_OFF_TIME_LIMIT) {
    sm_state = ENT_OFF;
    return;
  }

  if (v_in < int(VIN_OFF / VIN_MAX * VIN_SCALE)) {
    sm_state = ENT_DEPLETING;
    return;
  }
}

// KITT light effect to the left
LedPatternSegment depleting_pattern[] = {
    {{0, 0, 0, 255}, 0b0001, 50}, {{0, 0, 255, 0}, 0b0011, 50},
    {{0, 255, 0, 0}, 0b0110, 50}, {{255, 0, 0, 0}, 0b1100, 50},
    {{0, 0, 0, 0}, 0b1000, 50},   {{0, 0, 0, 0}, 0b0000, 750},
    {{0, 0, 0, 0}, 0b0000, 0},
};

void sm_state_ENT_DEPLETING() {
  led_blinker.set_pattern(depleting_pattern);
  sm_state = DEPLETING;
}

void sm_state_DEPLETING() {
  if (watchdog_limit && (watchdog_elapsed > watchdog_limit)) {
    sm_state = ENT_WATCHDOG_REBOOT;
    return;
  }

  if (shutdown_requested) {
    shutdown_requested = false;
    sm_state = ENT_SHUTDOWN;
    return;
  } else if (v_in > int(VIN_OFF / VIN_MAX * VIN_SCALE)) {
    sm_state = ENT_ON;
    return;
  } else if (v_supercap < power_off_vcap_voltage) {
    sm_state = ENT_OFF;
    return;
  }

  // kill the power if the host has been powered off for more than a second
  if (gpio_poweroff_elapsed > GPIO_OFF_TIME_LIMIT) {
    sm_state = ENT_OFF;
    return;
  }
}

elapsedMillis elapsed_shutdown;

// two longish blips, then a long pause
LedPatternSegment shutdown_pattern[] = {
    {{0, 0, 0, 0}, 0b1111, 200}, {{0, 0, 0, 0}, 0b0000, 100},
    {{0, 0, 0, 0}, 0b1111, 200}, {{0, 0, 0, 0}, 0b0000, 1000},
    {{0, 0, 0, 0}, 0b0000, 0},
};

void sm_state_ENT_SHUTDOWN() {
  led_blinker.set_pattern(shutdown_pattern);
  // ignore watchdog
  watchdog_limit = 0;
  elapsed_shutdown = 0;
  sm_state = SHUTDOWN;
}

void sm_state_SHUTDOWN() {
  if ((gpio_poweroff_elapsed > GPIO_OFF_TIME_LIMIT) ||
      (elapsed_shutdown > SHUTDOWN_WAIT_DURATION)) {
    sm_state = ENT_OFF;
  }
}

elapsedMillis elapsed_reboot;

// alternating blink pattern indicating something is wrong
LedPatternSegment watchdog_reboot_pattern[] = {
    {{255, 0, 255, 0}, 0b1111, 200},
    {{0, 255, 0, 255}, 0b1111, 200},
    {{0, 0, 0, 0}, 0b0000, 0},
};

void sm_state_ENT_WATCHDOG_REBOOT() {
  elapsed_reboot = 0;
  watchdog_limit = 0;
  Wire.end();  // need to do this before we turn off the power
  set_en5v_pin(false);
  led_blinker.set_pattern(watchdog_reboot_pattern);
  sm_state = WATCHDOG_REBOOT;
}

void sm_state_WATCHDOG_REBOOT() {
  if (elapsed_reboot > WATCHDOG_REBOOT_DURATION) {
    sm_state = BEGIN;
  }
}

elapsedMillis elapsed_off;

void sm_state_ENT_OFF() {
  Wire.end();  // need to do this before we turn off the power
  // delay(10);  // DEBUG
  set_en5v_pin(false);
  // delay(10);
  elapsed_off = 0;
  // in case we're not dead, set a blink pattern
  led_blinker.set_pattern(power_off_pattern);
  sm_state = OFF;
}

void sm_state_OFF() {
  if (elapsed_off > OFF_STATE_DURATION) {
    // if we're still alive, jump back to begin
    sm_state = BEGIN;
  }
}

void sm_state_ENT_SLEEP_SHUTDOWN() {
  led_blinker.set_pattern(shutdown_pattern);
  // ignore watchdog
  watchdog_limit = 0;
  elapsed_shutdown = 0;
  sm_state = SLEEP_SHUTDOWN;
}

void sm_state_SLEEP_SHUTDOWN() {
  if ((gpio_poweroff_elapsed > GPIO_OFF_TIME_LIMIT) ||
      (elapsed_shutdown > SHUTDOWN_WAIT_DURATION)) {
    sm_state = ENT_SLEEP;
  }
}

// two short blinks, then a long pause
LedPatternSegment sleep_pattern[] = {
    {{255, 255, 255, 255}, 0b1111, 100}, {{0, 0, 0, 0}, 0b0000, 200},
    {{255, 255, 255, 255}, 0b1111, 100}, {{0, 0, 0, 0}, 0b0000, 2000},
    {{0, 0, 0, 0}, 0b0000, 0},
};

void sm_state_ENT_SLEEP() {
  Wire.end();  // need to do this before we turn off the power
  set_en5v_pin(false);
  // we're not dead, set a blink pattern
  led_blinker.set_pattern(sleep_pattern);
  sm_state = SLEEP;
}

void sm_state_SLEEP() {
  if (rtc_wakeup_triggered || ext_wakeup_triggered) {
    sm_state = BEGIN;
  }
}

// function to run the state machine

void sm_run() {
  static StateType last_state = BEGIN;
  if (last_state != sm_state) {
    Serial.print("New state: ");
    Serial.println(state_names[sm_state]);
    last_state = sm_state;
  }
  if (sm_state < NUM_STATES) {
    // call the function for the state
    (*state_machine[sm_state])();
  } else {
    sm_state = BEGIN;  // FIXME: should we restart instead?
  }
}
