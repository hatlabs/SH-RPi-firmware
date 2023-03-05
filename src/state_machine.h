#ifndef _state_machine_H_
#define _state_machine_H_

// valid states for the power state machine

typedef enum {
  BEGIN,
  WAIT_VIN_ON,
  ENT_CHARGING,
  CHARGING,
  ENT_ON,
  ON,
  ENT_DEPLETING,
  DEPLETING,
  ENT_SHUTDOWN,
  SHUTDOWN,
  ENT_WATCHDOG_REBOOT,
  WATCHDOG_REBOOT,
  ENT_OFF,
  OFF,
  ENT_SLEEP_SHUTDOWN,
  SLEEP_SHUTDOWN,
  ENT_SLEEP,
  SLEEP,
  NUM_STATES
} StateType;

extern char *state_names[];

void sm_state_BEGIN();
void sm_state_WAIT_VIN_ON();
void sm_state_ENT_CHARGING();
void sm_state_CHARGING();
void sm_state_ENT_ON();
void sm_state_ON();
void sm_state_ENT_DEPLETING();
void sm_state_DEPLETING();
void sm_state_ENT_SHUTDOWN();
void sm_state_SHUTDOWN();
void sm_state_ENT_WATCHDOG_REBOOT();
void sm_state_WATCHDOG_REBOOT();
void sm_state_ENT_OFF();
void sm_state_OFF();
void sm_state_ENT_SLEEP_SHUTDOWN();
void sm_state_SLEEP_SHUTDOWN();
void sm_state_ENT_SLEEP();
void sm_state_SLEEP();

void sm_run();

StateType get_sm_state();
const char* get_sm_state_name();

#endif
