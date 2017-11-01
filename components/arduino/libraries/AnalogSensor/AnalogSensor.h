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

    static const uint8_t sensorNone       = 0;
    static const uint8_t sensorColdTempT2 = 1;
    static const uint8_t sensorColdTempT3 = 2;
    static const uint8_t sensorHotTempT2  = 3;
    static const uint8_t sensorHotTempT3  = 4;
    static const uint8_t sensorPress      = 5;

  private:
    uint8_t _sensorPin[ANALOG_CH_MAX];
    uint8_t _sensorType[ANALOG_CH_MAX];

    // 0 - 3.3 Volt = 0.05 volt/step
    static const uint8_t NUM_ADC_TABLE = 67;
    uint16_t _adcVoltage[NUM_ADC_TABLE] = {0,0,8,16,80,128,200,240,320,356,
                                430,473,560,596,672,736,790,870,912,1000,
                                1038,1108,1155,1230,1270,1346,1400,1470,1526,1608,
                                1677,1720,1793,1838,1910,1960,2032,2087,2167,2197,
                                2281,2311,2391,2459,2518,2589,2640,2727,2751,2831,
                                2899,2982,3043,3127,3183,3306,3408,3479,3607,3703,
                                3829,3919,4095,4095,4095,4095,4095};

    float calSensor(uint8_t, uint16_t); //type, value
    float getAdcVoltage(uint16_t);
    uint16_t getAvgAdc(uint8_t num, uint8_t ch);
    float valueRange(float value, float minVal, float maxVal);
};

#endif