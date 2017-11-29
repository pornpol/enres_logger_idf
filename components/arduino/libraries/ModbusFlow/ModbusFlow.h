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

    typedef struct __flowData
    {
      time_t mdt;
      float inst_flux;
      float ins_heat_flow;
      float tm1;
      float tm2;
      float signal_ratio;
      float reynolds;
      float q_str;
      float up_str;
      float down_str;
    } flowData;

    flowData fd;

    void begin(Stream &serial);
    void begin(Stream &serial, Stream &debug);
    void preTransmission(void (*)());
    void postTransmission(void (*)());

    /*_____READ HOLDING REGISTER_____*/
    uint8_t readFlowData(uint8_t, time_t, float*);
    
    /*_____READ DATA FROM BUFFER_____*/
    uint16_t getResponseBuffer(uint8_t);

    static const uint8_t ku8MBIllegalFunction            = 0x01;
    static const uint8_t ku8MBIllegalDataAddress         = 0x02;
    static const uint8_t ku8MBIllegalDataValue           = 0x03;
    static const uint8_t ku8MBSlaveDeviceFailure         = 0x04;
    static const uint8_t ku8MBSuccess                    = 0x00;
    static const uint8_t ku8MBInvalidSlaveID             = 0xE0;
    static const uint8_t ku8MBInvalidFunction            = 0xE1;
    static const uint8_t ku8MBResponseTimedOut           = 0xE2;
    static const uint8_t ku8MBInvalidCRC                 = 0xE3;

  private:
    Stream* _serial;
    Stream* _debug;
    static const uint8_t ku8MaxBufferSize                = 128;   ///< size of response/transmit buffers
    uint16_t _u16ResponseBuffer[ku8MaxBufferSize];               ///< buffer to store Modbus slave response; read via GetResponseBuffer()  
    //uint8_t _u8ResponseBufferLength;

    // preTransmission callback function; gets called before writing a Modbus message
    void (*_preTransmission)();
    // postTransmission callback function; gets called after a Modbus message has been sent
    void (*_postTransmission)();

    uint8_t masterTransaction(uint8_t slave, uint16_t startAddress, uint16_t readQty, uint8_t fnRead);
    float wordToFloat(uint16_t h, uint16_t l);
    uint32_t u16Tou32(uint16_t h, uint16_t l);

        // Modbus function codes for bit access
    static const uint8_t ku8MBReadCoils                  = 0x01; ///< Modbus function 0x01 Read Coils
    static const uint8_t ku8MBReadDiscreteInputs         = 0x02; ///< Modbus function 0x02 Read Discrete Inputs
    static const uint8_t ku8MBWriteSingleCoil            = 0x05; ///< Modbus function 0x05 Write Single Coil
    static const uint8_t ku8MBWriteMultipleCoils         = 0x0F; ///< Modbus function 0x0F Write Multiple Coils

    // Modbus function codes for 16 bit access
    static const uint8_t ku8MBReadHoldingRegisters       = 0x03; ///< Modbus function 0x03 Read Holding Registers
    static const uint8_t ku8MBReadInputRegisters         = 0x04; ///< Modbus function 0x04 Read Input Registers
    static const uint8_t ku8MBWriteSingleRegister        = 0x06; ///< Modbus function 0x06 Write Single Register
    static const uint8_t ku8MBWriteMultipleRegisters     = 0x10; ///< Modbus function 0x10 Write Multiple Registers
    static const uint8_t ku8MBMaskWriteRegister          = 0x16; ///< Modbus function 0x16 Mask Write Register
    static const uint8_t ku8MBReadWriteMultipleRegisters = 0x17; ///< Modbus function 0x17 Read Write Multiple Registers

    // Modbus timeout [milliseconds]
    static const uint16_t ku16MBResponseTimeout          = 500; ///< Modbus timeout [milliseconds]

};

#endif