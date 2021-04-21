#include "digital_io.h"

#include <avr/io.h>
#include <Arduino.h>

#include "globals.h"

bool read_pin(int pin) { 
  PORT_t* port = digitalPinToPortStruct(pin);
  return port->IN & digitalPinToBitMask(pin);
}

void update_pin(int pin, bool value) {
  PORT_t* port = digitalPinToPortStruct(pin);
  update_pin(port, digitalPinToBitMask(pin), value);
}

void update_pin(PORT_t* port, int pin_mask, bool value) {
  if (value) {
    port->OUTSET = pin_mask;
  } else {
    port->OUTCLR = pin_mask;
  }
}

void set_en5v_pin(bool state) { update_pin(EN5V_PIN, state); }

void set_pin_mode(int pin, bool output) {
  PORT_t* port = digitalPinToPortStruct(pin);
  if (output) {
    port->DIRSET = digitalPinToBitMask(pin);
  } else {
    port->DIRCLR = digitalPinToBitMask(pin);
  }
}
