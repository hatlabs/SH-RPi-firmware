#ifndef _blinker_H_
#define _blinker_H_

#include <elapsedMillis.h>

#include "digital_io.h"

#define BLINKER_PERIOD_SCALE 32768

class BaseBlinker {
 public:
  BaseBlinker(int pin);
  
 protected:
  PORT_t* port_;
  int pin_;
  int pin_mask_;
  bool state_ = false;
  elapsedMillis elapsed;
};

class PeriodicBlinker : public BaseBlinker {
 public:
  PeriodicBlinker(int pin, unsigned int period);
  void set_period(unsigned int period) { this->period = period; }

 protected:
  unsigned int period;
};

class EvenBlinker : public PeriodicBlinker {
 public:
  EvenBlinker(int pin, unsigned int period);
  void tick();
};

class RatioBlinker : public PeriodicBlinker {
 public:
  // ratio has a scaling factor of 32768
  RatioBlinker(int pin, unsigned int period, unsigned int ratio = 0);
  void tick();
  void set_ratio(unsigned int ratio) { this->ratio = ratio; }

 protected:
  unsigned int ratio;
};

class PatternBlinker : public BaseBlinker {
 public:
  PatternBlinker(int pin, int pattern[]);
  void tick();
  void set_pattern(int pattern[]);
  void restart();

 protected:
  int* pattern;
  unsigned int pattern_ptr = 0;
};

#endif
