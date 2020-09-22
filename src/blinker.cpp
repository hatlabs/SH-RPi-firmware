#include "blinker.h"
#include "digital_io.h"

BaseBlinker::BaseBlinker(int port, int pin) : port{port}, pin{pin} {}

void BaseBlinker::set_state(volatile uint8_t *port_a_state, volatile uint8_t *port_b_state, bool state) {
  this->state = state;
  auto port_state = this->port ? port_b_state : port_a_state;
  update_port(port_state, pin, state);
}


PeriodicBlinker::PeriodicBlinker(int port, int pin, unsigned int period)
: BaseBlinker(port, pin),
  period{period} {}

EvenBlinker::EvenBlinker(int port, int pin, unsigned int period)
: PeriodicBlinker(port, pin, period) {}

void EvenBlinker::tick(volatile uint8_t* port_a_state, volatile uint8_t* port_b_state) {
  if (elapsed > period) {
    auto port_state = this->port ? port_b_state : port_a_state;
    elapsed = 0;
    state = !state;
    return update_port(port_state, pin, state);
  }
}

RatioBlinker::RatioBlinker(int port, int pin, unsigned int period, unsigned int ratio)
: PeriodicBlinker(port, pin, period),
  ratio{ratio} {}

void RatioBlinker::tick(volatile uint8_t* port_a_state, volatile uint8_t* port_b_state) {
  int onDuration = (long)ratio * period / 32768;
  int offDuration = max(0, period - onDuration);
  unsigned int refDuration = state==false ? offDuration : onDuration;
  if (elapsed > refDuration) {
    auto port_state = this->port ? port_b_state : port_a_state;
    elapsed = 0;
    state = !state;
    return update_port(port_state, pin, state);
  }
}


PatternBlinker::PatternBlinker(int port, int pin, int pattern[])
: BaseBlinker(port, pin),
  pattern{pattern} {}

void PatternBlinker::set_pattern(int pattern[]) { 
  this->pattern = pattern; 
  state = false;
  pattern_ptr = 0;
}

void PatternBlinker::tick(volatile uint8_t* port_a_state, volatile uint8_t* port_b_state) {
  if (elapsed > pattern[pattern_ptr]) {
    auto port_state = this->port ? port_b_state : port_a_state;
    elapsed = 0;
    pattern_ptr++;
    // loop to zero if we reached the end
    if (pattern[pattern_ptr]==-1) {
      pattern_ptr = 0;
      state = false;
    }
    state = !state;
    
    update_port(port_state, pin, state);
  }
}

void PatternBlinker::restart() {
  state = false;
  pattern_ptr = 0;
  elapsed = 0;
}
