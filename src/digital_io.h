#ifndef _digital_io_H_
#define _digital_io_H_

#include <stdint.h>

#include <Arduino.h>

/**
 * @brief Write a pin value.
 * 
 * Similar to Arduino digitalWrite but assumes the pin is already
 * in a correct state.
 * 
 * @param pin Pin number
 * @param value Value to set
 */
void update_pin(int pin, bool value);

void update_pin(PORT_t* port, int pin_mask, bool value);

void set_en5v_pin(bool state);

/**
 * @brief Read a pin value.
 * 
 * This is pretty much same as the Arduino standard digitalRead except that
 * it skips a lot of safety checks present in digitalRead
 * 
 * @param pin Pin number
 * @return pin boolean state
 */
bool read_pin(int pin);

#endif
