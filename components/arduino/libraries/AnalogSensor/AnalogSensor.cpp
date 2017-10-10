#include "AnalogSensor.h"

AnalogSensor::AnalogSensor()
{

}

bool AnalogSensor::begin(uint8_t pins[ANALOG_CH_MAX], uint8_t types[ANALOG_CH_MAX])
{
  for(uint8_t i=0; i<ANALOG_CH_MAX; i++)
  {
    _sensorPin[i] = pins[i];
    _sensorType[i] = types[i];
  }
  return true;
}

float AnalogSensor::getSensor(uint8_t ch)
{
  return calSensor(_sensorType[ch], analogRead(_sensorPin[ch]));
}

float AnalogSensor::calSensor(uint8_t type, uint16_t value)
{
  float calValue;
  float calVolt;
  float calRes;


  switch(type)
  {
    case sensorNone : // Not Connected
      calValue = value; 
      break;

    case sensorTemp : // Temp
      //Voltage = analogread*(3.3/1023)
      //resistor = ((20*1000*3.3)/Voltage0)-(20*1000)
      //temperature = ((-24.23)*log(resistor))+248.33
      calVolt = value*(5/4095);
      calRes = ((20*1000*5)/calVolt)-(20*1000);
      calValue = ((-24.23)*log(calRes))+248.33; // Update to type 3
      break;

    case sensorPress : // Press
      //Voltage = analogread*(3.3/1023)
      //pressure = (Voltage/0.165)-4
      calVolt = value*(5/4095);
      calValue = (calVolt/0.165)-4; 
      break;

    default : 
      calValue = value; 
      break;
  }

  return calValue;
}