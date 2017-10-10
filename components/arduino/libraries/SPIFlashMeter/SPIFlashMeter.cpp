#include "SPIFlashMeter.h"

SPIFlashMeter::SPIFlashMeter()
{

}

bool SPIFlashMeter::begin(uint8_t recordSize, Stream& debug)
{
  _recordSize = recordSize;
  _debug = &debug;

  if(!_flash.begin())
  {
    _debug->println("Init SPI Flash Not OK!!");
    return false;
  }
  else _debug->println("Init SPI Flash OK");
  
  _debug->printf("SPI FLASH JEDEC ID: %04lxh\r\n", (unsigned long)_flash.getJEDECID());
  _debug->printf("SPI FLASH SIZE: %d BYTES\r\n", _flash.getCapacity());

  _maxRecord = (_flash.getCapacity() - _reserveSize)/recordSize;

  return true;
}

//buff 32 or 64
bool SPIFlashMeter::writeMeterData(uint32_t index, uint8_t *buff, uint32_t buff_size)
{
  uint32_t addr = (index%_maxRecord)*_recordSize + _reserveSize; //Cal Address from Chip Size, NumMeter
  uint8_t numRec = buff_size/_recordSize;

  for(uint8_t i=0; i<numRec; i++)
  {
    //first index of page
    if((index+i)%(_sectorSize/_recordSize) == 0)
    {
      //erase that _sector
      _flash.eraseSector(((index+i)%_maxRecord)*_recordSize + _reserveSize);
      _debug->printf("Erase Address : %d\r\n", ((index+i)%_maxRecord)*_recordSize);
    }
  }
  //Erase page that write in first time
  return _flash.writeByteArray(addr, buff, buff_size, true);
}

//buff 32 or 64s
bool SPIFlashMeter::readMeterData(uint32_t index, uint8_t *buff, uint32_t buff_size)
{
  uint32_t addr = (index%_maxRecord)*_recordSize + _reserveSize; //Cal Address from Chip Size, NumMeter

  return _flash.readByteArray(addr, buff, buff_size);
}

uint32_t SPIFlashMeter::getMaxRecord()
{
  return _maxRecord;
}

bool SPIFlashMeter::writeConfigFlash(String config)
{
  uint8_t i;
  char buff[_configSize];

  _flash.eraseSector(_configAddress);

  config.toCharArray(buff, config.length());

  for(i=0; i<=(config.length()/_pageSize); i++)
  {
    _flash.writeCharArray(i, 0, &buff[i*_pageSize], _pageSize);
  }
  
  return true;
}

String SPIFlashMeter::readConfigFlash()
{
  String config;
  char charConfig[_configSize];
  
  _flash.readCharArray(0, charConfig, _configSize, false);

  config = String(charConfig);

  return config;
}