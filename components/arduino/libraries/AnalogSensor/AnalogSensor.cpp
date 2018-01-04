#include "AnalogSensor.h"

AnalogSensor::AnalogSensor()
{
  pinMode(ADC_SPI_CS, OUTPUT);
  
  // set initial PIN state
  digitalWrite(ADC_SPI_CS, HIGH);
}

bool AnalogSensor::begin(uint8_t pins[ANALOG_CH_MAX], uint8_t types[ANALOG_CH_MAX], uint8_t adcExt)
{
  // SPISettings settings(1600000, MSBFIRST, SPI_MODE0);
  // SPI.begin();
  // SPI.beginTransaction(settings);

  for(uint8_t i=0; i<ANALOG_CH_MAX; i++)
  {
    _sensorPin[i] = pins[i];
    _sensorType[i] = types[i];
  }

  _adcExt = adcExt;

  return true;
}

float AnalogSensor::getSensor(uint8_t ch)
{
  //return calSensor(_sensorType[ch], analogRead(_sensorPin[ch])); // Get only 1 value
  return calSensor(_sensorType[ch], getAvgAdc(16, ch)); // Get avg 20-4 Value
}

float AnalogSensor::calSensor(uint8_t type, uint16_t value)
{
  float calValue;
  float calVolt;
  float calRes;

  //Serial.printf("Value : %d\r\n", value);

  switch(type)
  {
    case sensorNone : // 0: Not Use
      calValue = getAdcVoltage(value); 
      break;

    case sensorColdTempT2 : // 1: Cold Water Temp Type2
      calVolt = getAdcVoltage(value);
      calRes = ((20*1000*5)/calVolt)-(20*1000);
      calValue = valueRange(((-20.73)*log(calRes))+215.43, -5, 24);
      break;

    case sensorHotTempT3 : // 4: Hot Water Temp Type3
      calVolt = getAdcVoltage(value);
      calRes = ((5.6*1000*5)/calVolt)-(5.6*1000);
      calValue = valueRange(((-25.45)*log(calRes))+259.54, 15, 45);
      break;

    case sensorPress : // 5: Pressure
      calVolt = getAdcVoltage(value);
      calValue = (calVolt/0.165)-4; 
      break;

    case sensorTemp420 : // 6: Temp 4-20 0-50V
    calVolt = getAdcVoltage(value);
    calValue = ((calVolt-0.66)*50)/(3.3-0.66); 
    break;

    default : 
      calValue = getAdcVoltage(value); 
      break;
  }

  return calValue;
}

float AnalogSensor::getAdcVoltage(uint16_t value)
{
  if(_adcExt == 1)
    return (value/4095.0)*3.3;

  for(uint8_t i=0; i<NUM_ADC_TABLE; i++)
  {
    if(value == 0) return 0;

    if(_adcVoltage[i] >= value)
    {
      return ((0.05*(i-1)) + (((float)(value-_adcVoltage[i-1])/(float)(_adcVoltage[i]-_adcVoltage[i-1]))*0.05));
    }
  }
  return 3.3;
}

uint16_t AnalogSensor::getAvgAdc(uint8_t num, uint8_t ch)
{
  uint16_t val;
  uint32_t sumVal = 0;
  uint16_t minVal = 4095;
  uint16_t minnVal = 4095;
  uint16_t minnnVal = 4095;
  uint16_t maxVal = 0;
  uint16_t maxxVal = 0;
  uint16_t maxxxVal = 0;
  uint16_t avgVal = 0;

  for(uint8_t i=0; i<num; i++)
  {
    if(_adcExt == 0)
    {
      val = analogRead(_sensorPin[ch]);
    }
    else
    {
      SPISettings settings(1600000, MSBFIRST, SPI_MODE0);
      //SPI.begin();
      SPI.beginTransaction(settings);
      val = adc.read((MCP3208::Channel)(0b1000+ch));
      //Serial.printf("Raw Ext ADC CH%d : %d\r\n", ch, val);
    }

    if(val < minVal)
    {
      if(val < minnVal)
      {
        minVal = minnVal;
        if(val < minnnVal)
        {
          minnVal = minnnVal;
          minnnVal = val;
        }
        else
          minnVal = val;
      }
      else
        minVal = val;
    }

    if(val > maxVal)
    {
      if(val > maxxVal)
      {
        maxVal = maxxVal;
        if(val > maxxxVal)
        {
          maxxVal = maxxxVal;
          maxxxVal = val;
        }
        else
          maxxVal = val;
      }
      else
        maxVal = val;
    }

    sumVal += val;

    // Re-read
    // if((i > 6) && (maxVal - minVal) > 40)
    // {
    //   Serial.printf("Retry ADC CH%d Read Cnt %i : Min %d ,Max %d\r\n", ch, i, minVal, maxVal);
    //   i = 0;
    //   uint32_t sumVal = 0;
    //   uint16_t minVal = 4095;
    //   uint16_t minnVal = 4095;
    //   uint16_t minnnVal = 4095;
    //   uint16_t maxVal = 0;
    //   uint16_t maxxVal = 0;
    //   uint16_t maxxxVal = 0;
    // }
    delay(2);
  }

  avgVal = (sumVal-minnnVal-minnVal-minVal-maxVal-maxxVal-maxxxVal)/(num-6);
  
  //Serial.printf("Raw Ext ADC CH%d : Minnn %d, Minn %d, Max %d, Maxx %d, Avg %d, Min %d, Max %d\r\n", ch, minnnVal, minnVal, maxxVal, maxxxVal, avgVal, minVal, maxVal);

  return avgVal;
}

float AnalogSensor::valueRange(float value, float minVal, float maxVal)
{
  if(value < minVal) return minVal;
  if(value > maxVal) return maxVal;

  return value;
}