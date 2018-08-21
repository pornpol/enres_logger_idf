#ifndef SPIFlashMeter_h
#define SPIFlashMeter_h

#include "Arduino.h"
#include "SPIFlash.h"

class SPIFlashMeter
{
  public:
    SPIFlashMeter();
    bool begin(uint8_t recordSize, Stream& debug);
    bool writeMeterData(uint32_t, uint8_t*, uint32_t);
    bool readMeterData(uint32_t, uint8_t*, uint32_t);
    bool writeSensorData(uint32_t, uint8_t*, uint32_t);
    bool readSensorData(uint32_t, uint8_t*, uint32_t);
    bool writeFlowData(uint32_t, uint8_t*, uint32_t);
    bool readFlowData(uint32_t, uint8_t*, uint32_t);
    uint32_t getMaxRecord();

    bool writeConfigFlash(String);
    String readConfigFlash();

  private:
    Stream* _debug;
    SPIFlash _flash;
    uint8_t _recordSize;
    uint32_t _maxRecord;
    const uint32_t _pageSize = 256;
    const uint32_t _sectorSize = 4096;

    const uint32_t _configSize = 4096;
    const uint8_t _configAddress = 0;
    const uint32_t _reserveSize = 8192;
};

#endif