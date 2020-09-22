#ifndef _blinker_H_
#define _blinker_H_

#include <elapsedMillis.h>

#include "digital_io.h"

class BaseBlinker {
 public:
  BaseBlinker(int port, int pin);
  void set_state(volatile uint8_t* port_a_state, volatile uint8_t* port_b_state,
                 bool state);

 protected:
  int port;
  int pin;
  bool state = false;
  elapsedMillis elapsed;
};

class PeriodicBlinker : public BaseBlinker {
 public:
  PeriodicBlinker(int port, int pin, unsigned int period);
  void set_period(unsigned int period) { this->period = period; }

 protected:
  unsigned int period;
};

class EvenBlinker : public PeriodicBlinker {
 public:
  EvenBlinker(int port, int pin, unsigned int period);
  void tick(volatile uint8_t* port_a_state, volatile uint8_t* port_b_state);
};

class RatioBlinker : public PeriodicBlinker {
 public:
  // ratio has a scaling factor of 32768
  RatioBlinker(int port, int pin, unsigned int period, unsigned int ratio = 0);
  void tick(volatile uint8_t* port_a_state, volatile uint8_t* port_b_state);
  void set_ratio(unsigned int ratio) { this->ratio = ratio; }

 protected:
  unsigned int ratio;
};

class PatternBlinker : public BaseBlinker {
 public:
  PatternBlinker(int port, int pin, int pattern[]);
  void tick(volatile uint8_t* port_a_state, volatile uint8_t* port_b_state);
  void set_pattern(int pattern[]);
  void restart();

 protected:
  int* pattern;
  unsigned int pattern_ptr = 0;
};

#endif
