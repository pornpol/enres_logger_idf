#ifndef AnalogSensor_h
#define AnalogSensor_h

#include "Arduino.h"
#include "Math.h"

#define ANALOG_CH_MAX 8

class AnalogSensor
{
  public:
    AnalogSensor();
    bool begin(uint8_t[ANALOG_CH_MAX], uint8_t[ANALOG_CH_MAX]); // ch & type
    float getSensor(uint8_t);

    static const uint8_t sensorNone   = 0;
    static const uint8_t sensorTemp   = 1;
    static const uint8_t sensorPress  = 2;

  private:
    uint8_t _sensorPin[ANALOG_CH_MAX];
    uint8_t _sensorType[ANALOG_CH_MAX];
    
    float calSensor(uint8_t, uint16_t); //type, value
};

#endif