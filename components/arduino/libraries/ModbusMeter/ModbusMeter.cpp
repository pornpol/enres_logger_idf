#include "ModbusMeter.h"
#include <esp_task_wdt.h>

ModbusMeter::ModbusMeter(void)
{
  _preTransmission = 0;
  _postTransmission = 0;
}

void ModbusMeter::begin(Stream &serial)
{
  _serial = &serial;
}

void ModbusMeter::begin(Stream &serial, Stream &debug)
{
  _serial = &serial;
  _debug = &debug;
  _debug->println("Init Modbus Meter");
}

void ModbusMeter::preTransmission(void (*preTransmission)())
{
  _preTransmission = preTransmission;
}

void ModbusMeter::postTransmission(void (*postTransmission)())
{
  _postTransmission = postTransmission;
}

uint16_t ModbusMeter::getResponseBuffer(uint8_t u8Index)
{
  if (u8Index < ku8MaxBufferSize)
  {
    return _u16ResponseBuffer[u8Index];
  }
  else
  {
    return 0xFFFF;
  }
}

uint8_t ModbusMeter::masterTransaction(uint8_t slave, uint16_t startAddress, uint16_t readQty, uint8_t fnRead)
{
  uint16_t u16CRC;
  uint8_t u8ModbusADU[256];
  uint8_t u8ModbusADUSize = 0;
  uint8_t i;

  uint32_t u32StartTime;
  uint8_t u8BytesLeft = 8;
  uint8_t u8MBStatus = ku8MBSuccess;

  uint8_t u8MBFunction = fnRead;
  //uint8_t u8MBFunction = ku8MBReadHoldingRegisters;

  u8ModbusADU[u8ModbusADUSize++] = slave;
  // MODBUS function = readHoldingRegister
  u8ModbusADU[u8ModbusADUSize++] = u8MBFunction;
  // MODBUS Address
  u8ModbusADU[u8ModbusADUSize++] = highByte(startAddress);
  u8ModbusADU[u8ModbusADUSize++] = lowByte(startAddress);
  // MODBUS Data Size
  u8ModbusADU[u8ModbusADUSize++] = highByte(readQty);
  u8ModbusADU[u8ModbusADUSize++] = lowByte(readQty);

  // calculate CRC
  u16CRC = 0xFFFF;
  for (i = 0; i < (u8ModbusADUSize); i++)
  {
    u16CRC = crc16_update(u16CRC, u8ModbusADU[i]);
  }
  u8ModbusADU[u8ModbusADUSize++] = lowByte(u16CRC);
  u8ModbusADU[u8ModbusADUSize++] = highByte(u16CRC);
  u8ModbusADU[u8ModbusADUSize] = 0;

  // flush receive buffer before transmitting request
  //while (_serial->read() != -1);
  // while(_serial->available())
  // {
  //   _serial->read();
  // }

  // transmit request
  if (_preTransmission)
  {
    _preTransmission();
  }
  uint8_t* data = (uint8_t*) "test";
  for (i = 0; i < u8ModbusADUSize; i++)
  {
    //_serial->write(u8ModbusADU[i]);
    uart_write_bytes(UART_NUM_2, (const char*)&u8ModbusADU[i], 1);
    if(_debug)
    {
      _debug->write(u8ModbusADU[i]);
    }
  }
  
  u8ModbusADUSize = 0;
  //_serial->flush();    // flush transmit buffer

  if (_postTransmission)
  {
    delay(10);
    _postTransmission();
  }

  // loop until we run out of time or bytes, or an error occurs
  u32StartTime = millis();
  while (u8BytesLeft && !u8MBStatus)
  {
    //if (_serial->available())
    //{
      //u8ModbusADU[u8ModbusADUSize++] = _serial->read();
      if(uart_read_bytes(UART_NUM_2, &u8ModbusADU[u8ModbusADUSize++], 1, 1000))
      {
        u8BytesLeft--;
      }
      //_debug->printf("Char %c \r\n", u8ModbusADU[u8ModbusADUSize-1]);
    //}

    // evaluate slave ID, function code once enough bytes have been read
    if (u8ModbusADUSize == 5)
    {
      // verify response is for correct Modbus slave
      if (u8ModbusADU[0] != slave)
      {
        u8MBStatus = ku8MBInvalidSlaveID;
        break;
      }
      
      // verify response is for correct Modbus function code (mask exception bit 7)
      if ((u8ModbusADU[1] & 0x7F) != u8MBFunction)
      {
        u8MBStatus = ku8MBInvalidFunction;
        break;
      }
      
      // check whether Modbus exception occurred; return Modbus Exception Code
      if (bitRead(u8ModbusADU[1], 7))
      {
        u8MBStatus = u8ModbusADU[2];
        break;
      }
      
      // evaluate returned Modbus function code
      switch(u8ModbusADU[1])
      {
        case ku8MBReadCoils:
        case ku8MBReadDiscreteInputs:
        case ku8MBReadInputRegisters:
        case ku8MBReadHoldingRegisters:
        case ku8MBReadWriteMultipleRegisters:
          u8BytesLeft = u8ModbusADU[2];
          break;
          
        case ku8MBWriteSingleCoil:
        case ku8MBWriteMultipleCoils:
        case ku8MBWriteSingleRegister:
        case ku8MBWriteMultipleRegisters:
          u8BytesLeft = 3;
          break;
          
        case ku8MBMaskWriteRegister:
          u8BytesLeft = 5;
          break;
      }
    }

    if ((millis() - u32StartTime) > ku16MBResponseTimeout)
    {
      u8MBStatus = ku8MBResponseTimedOut;
    }
  }
  // verify response is large enough to inspect further
  if (!u8MBStatus && u8ModbusADUSize >= 5)
  {
    // calculate CRC
    u16CRC = 0xFFFF;
    for (i = 0; i < (u8ModbusADUSize - 2); i++)
    {
      u16CRC = crc16_update(u16CRC, u8ModbusADU[i]);
    }
    
    // verify CRC
    if (!u8MBStatus && (lowByte(u16CRC) != u8ModbusADU[u8ModbusADUSize - 2] ||
      highByte(u16CRC) != u8ModbusADU[u8ModbusADUSize - 1]))
    {
      u8MBStatus = ku8MBInvalidCRC;
    }
  }

  // disassemble ADU into words
  if (!u8MBStatus)
  {
    // evaluate returned Modbus function code
    switch(u8ModbusADU[1])
    {
      case ku8MBReadCoils:
      case ku8MBReadDiscreteInputs:
        // load bytes into word; response bytes are ordered L, H, L, H, ...
        for (i = 0; i < (u8ModbusADU[2] >> 1); i++)
        {
          if (i < ku8MaxBufferSize)
          {
            _u16ResponseBuffer[i] = word(u8ModbusADU[2 * i + 4], u8ModbusADU[2 * i + 3]);
          }
          
          //_u8ResponseBufferLength = i;
        }
        
        // in the event of an odd number of bytes, load last byte into zero-padded word
        if (u8ModbusADU[2] % 2)
        {
          if (i < ku8MaxBufferSize)
          {
            _u16ResponseBuffer[i] = word(0, u8ModbusADU[2 * i + 3]);
          }
          
          //_u8ResponseBufferLength = i + 1;
        }
        break;
        
      case ku8MBReadInputRegisters:
      case ku8MBReadHoldingRegisters:
      case ku8MBReadWriteMultipleRegisters:
        // load bytes into word; response bytes are ordered H, L, H, L, ...
        for (i = 0; i < (u8ModbusADU[2] >> 1); i++)
        {
          if (i < ku8MaxBufferSize)
          {
            _u16ResponseBuffer[i] = word(u8ModbusADU[2 * i + 3], u8ModbusADU[2 * i + 4]);
          }
          
          //_u8ResponseBufferLength = i;
        }
        break;
    }
  }
  return u8MBStatus;
}

uint8_t ModbusMeter::readMeterData(uint8_t index, uint8_t slave, uint8_t slaveIndex, uint8_t mType, time_t mdt, float* adj)
{
  uint8_t result = 0x00;
  // Select Meter Type
  switch(mType)
  {
    case dts353: // 3 Phase Meter
      result = masterTransaction(slave, 0x000e, 6, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].v0 = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[7];
      md[index].v1 = wordToFloat(getResponseBuffer(2), getResponseBuffer(3))*adj[8];
      md[index].v2 = wordToFloat(getResponseBuffer(4), getResponseBuffer(5))*adj[9];

      result |= masterTransaction(slave, 0x0016, 8, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].i0 = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[4];
      md[index].i1 = wordToFloat(getResponseBuffer(2), getResponseBuffer(3))*adj[5];
      md[index].i2 = wordToFloat(getResponseBuffer(4), getResponseBuffer(5))*adj[6];
      md[index].watt = wordToFloat(getResponseBuffer(6), getResponseBuffer(7))*adj[0];

      result |= masterTransaction(slave, 0x0034, 2, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].pf = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[2];

      result |= masterTransaction(slave, 0x00100, 2, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].wattHour = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[1];

      result |= masterTransaction(slave, 0x00118, 2, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].varh = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[3];

      md[index].mdt = mdt;
      
      break;

    case eastron:
      result = masterTransaction(slave, 0x0000 + (2000*slaveIndex), 12, ku8MBReadInputRegisters);
      if(result) return result;
      md[index].v0 = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[7];
      md[index].v1 = wordToFloat(getResponseBuffer(2), getResponseBuffer(3))*adj[8];
      md[index].v2 = wordToFloat(getResponseBuffer(4), getResponseBuffer(5))*adj[9];
      md[index].i0 = wordToFloat(getResponseBuffer(6), getResponseBuffer(7))*adj[4];
      md[index].i1 = wordToFloat(getResponseBuffer(8), getResponseBuffer(9))*adj[5];
      md[index].i2 = wordToFloat(getResponseBuffer(10), getResponseBuffer(11))*adj[6];

      result |= masterTransaction(slave, 0x0034 + (2000*slaveIndex), 2, ku8MBReadInputRegisters);
      if(result) return result;
      md[index].watt = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[0];

      result |= masterTransaction(slave, 0x003E + (2000*slaveIndex), 2, ku8MBReadInputRegisters);
      if(result) return result;
      md[index].pf = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[2];

      result |= masterTransaction(slave, 0x0156 + (2000*slaveIndex), 4, ku8MBReadInputRegisters);
      if(result) return result;
      md[index].wattHour = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[1];
      md[index].varh = wordToFloat(getResponseBuffer(2), getResponseBuffer(3))*adj[3];

      md[index].mdt = mdt;

      break;
    
    case iem3255:
      result |= masterTransaction(slave, 3000, 6, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].i0 = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[4];
      md[index].i1 = wordToFloat(getResponseBuffer(2), getResponseBuffer(3))*adj[5];
      md[index].i2 = wordToFloat(getResponseBuffer(4), getResponseBuffer(5))*adj[6];

      result = masterTransaction(slave, 3020, 6, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].v0 = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[7];
      md[index].v1 = wordToFloat(getResponseBuffer(2), getResponseBuffer(3))*adj[8];
      md[index].v2 = wordToFloat(getResponseBuffer(4), getResponseBuffer(5))*adj[9];
      
      result |= masterTransaction(slave, 3060, 2, ku8MBReadHoldingRegisters);
      if(result) return result;      
      md[index].watt = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[0];

      result |= masterTransaction(slave, 3084, 2, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].pf = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[2];

      result |= masterTransaction(slave, 3204, 2, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].wattHour = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[1];

      result |= masterTransaction(slave, 3220, 2, ku8MBReadHoldingRegisters);
      if(result) return result;
      md[index].varh = wordToFloat(getResponseBuffer(0), getResponseBuffer(1))*adj[3];

      md[index].mdt = mdt;
      
      break;
  }

  return result;
}

float ModbusMeter::wordToFloat(uint16_t h, uint16_t l)
{
  typedef union
  {
  float number;
  uint16_t wd[2];
  } FLOATUNION_t;

  FLOATUNION_t myFloat;

  myFloat.wd[0] = l;
  myFloat.wd[1] = h;

  return myFloat.number;
}

uint32_t ModbusMeter::u16Tou32(uint16_t h, uint16_t l)
{
  typedef union
  {
  uint32_t number;
  uint16_t wd[2];
  } FLOATUNION_t;

  FLOATUNION_t myU32;
  
  myU32.wd[0] = h;
  myU32.wd[1] = l;

  return myU32.number;
}