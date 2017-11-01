#include "HardwareWDT.h"

HardwareWDT::HardwareWDT()
{
}

void HardwareWDT::begin(uint8_t kdPin, uint8_t enPin)
{
  _kdPin = kdPin;
  _enPin = enPin;

  pinMode(_kdPin, OUTPUT);
  pinMode(_enPin, OUTPUT);

  digitalWrite(_kdPin, LOW);
  digitalWrite(_enPin, LOW);
}

void HardwareWDT::kickDog()
{
  digitalWrite(_kdPin, HIGH);
  delay(1);
  digitalWrite(_kdPin, LOW);
}

void HardwareWDT::enable()
{
  digitalWrite(_enPin, LOW);
}

void HardwareWDT::disable()
{
  digitalWrite(_enPin, HIGH);
}