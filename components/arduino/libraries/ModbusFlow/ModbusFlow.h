#ifndef ModbusFlow_h
#define ModbusFlow_h

/* _____STANDARD INCLUDES____________________________________________________ */
// include types & constants of Wiring core API
#include "Arduino.h"

/* _____UTILITY MACROS_______________________________________________________ */


/* _____PROJECT INCLUDES_____________________________________________________ */
// functions to calculate Modbus Application Data Unit CRC
#include "util/crc16.h"

// functions to manipulate words
#include "util/word.h"

#include <driver/uart.h>

class ModbusFlow
{
  public:
    ModbusFlow();

  private:
};

#endif