#include "state_machine.h"

#include "digital_io.h"
#include "globals.h"

// take care to have all enum values of StateType present
void (*state_machine[])(void) = {
    sm_state_BEGIN,
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
    sm_state_SLEEP
};

StateType sm_state = BEGIN;

StateType get_sm_state() { return sm_state; }

// PATTERN: *___________________
int off_pattern[] = {50, 950, -1};

void sm_state_BEGIN() {
  set_en5v_pin(false);

  i2c_register = 0xff;
  watchdog_limit = 0;
  gpio_poweroff_elapsed = 0;

  status_blinker.set_pattern(off_pattern);

  sm_state = WAIT_VIN_ON;
}

void sm_state_WAIT_VIN_ON() {
  // never start if DC input voltage is not present
  if (v_in >= int(VIN_OFF / VIN_MAX * VIN_SCALE)) {
    sm_state = ENT_CHARGING;
  }
}

// PATTERN: *************_*_*_*_
int charging_pattern[] = {650, 50, 50, 50, 50, 50, 50, 50, -1};

void sm_state_ENT_CHARGING() {
  status_blinker.set_pattern(charging_pattern);
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

// PATTERN: ********************
int solid_on_pattern[] = {1000, 0, -1};

void sm_state_ENT_ON() {
  set_en5v_pin(true);
  status_blinker.set_pattern(solid_on_pattern);
  sm_state = ON;
}

// PATTERN: *******************_
int watchdog_enabled_pattern[] = {950, 50, -1};

void sm_state_ON() {
  if (watchdog_value_changed) {
    if (watchdog_limit) {
      status_blinker.set_pattern(watchdog_enabled_pattern);
    } else {
      status_blinker.set_pattern(solid_on_pattern);
    }
    watchdog_value_changed = false;
  }
  if (watchdog_limit && (watchdog_elapsed > watchdog_limit)) {
    sm_state = ENT_WATCHDOG_REBOOT;
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
  }
}

// PATTERN: *_*_*_*_____________
int draining_pattern[] = {50, 50, 50, 50, 50, 50, 50, 650, -1};

void sm_state_ENT_DEPLETING() {
  status_blinker.set_pattern(draining_pattern);
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

// PATTERN: ****____
int shutdown_pattern[] = {200, 200, -1};

void sm_state_ENT_SHUTDOWN() {
  status_blinker.set_pattern(shutdown_pattern);
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

// PATTERN: *_
int watchdog_pattern[] = {50, 50, -1};

void sm_state_ENT_WATCHDOG_REBOOT() {
  elapsed_reboot = 0;
  watchdog_limit = 0;
  set_en5v_pin(false);
  status_blinker.set_pattern(watchdog_pattern);
  sm_state = WATCHDOG_REBOOT;
}

void sm_state_WATCHDOG_REBOOT() {
  if (elapsed_reboot > WATCHDOG_REBOOT_DURATION) {
    sm_state = BEGIN;
  }
}

elapsedMillis elapsed_off;

void sm_state_ENT_OFF() {
  set_en5v_pin(false);
  // in case we're not dead, set a blink pattern
  status_blinker.set_pattern(off_pattern);
  sm_state = OFF;
}

void sm_state_OFF() {
  if (elapsed_off > OFF_STATE_DURATION) {
    // if we're still alive, jump back to begin
    sm_state = BEGIN;
  }
}

void sm_state_ENT_SLEEP_SHUTDOWN() {
  status_blinker.set_pattern(shutdown_pattern);
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

void sm_state_ENT_SLEEP() {
  set_en5v_pin(false);
  // we're not dead, set a blink pattern
  status_blinker.set_pattern(off_pattern);
  sm_state = SLEEP;
}

void sm_state_SLEEP() {
  if (rtc_wakeup_triggered || ext_wakeup_triggered) {
    sm_state = BEGIN;
  }
}


// function to run the state machine

void sm_run() {
  if (sm_state < NUM_STATES) {
    // call the function for the state
    (*state_machine[sm_state])();
  } else {
    sm_state = BEGIN;  // FIXME: should we restart instead?
  }
}
