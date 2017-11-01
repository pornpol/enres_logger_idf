#ifndef HardwareWDT_h
#define HardwareWDT_h

#include "Arduino.h"

class HardwareWDT
{
  public:
    HardwareWDT();
    void begin(uint8_t, uint8_t);
    void kickDog();
    void enable();
    void disable();

  private:
    uint8_t _kdPin;
    uint8_t _enPin;
};

#endif