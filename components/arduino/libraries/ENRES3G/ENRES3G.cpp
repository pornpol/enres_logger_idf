#include "ENRES3G.h"

ENRES3G::ENRES3G()
{
  
}

void ENRES3G::begin(Stream &serial, Stream &debug, String apn, String user, String pass)
{
  _serial = &serial;
  _debug = &debug;
  _apn = apn;
  _user = user;
  _pass = pass;

  //Start Connection
  Serial.println(F("UC20"));
  gsm.begin(_serial);
  
  gsm.SetPowerKeyPin(13);
  
  gsm.PowerOn(); 
  /*
  while(gsm.WaitReady()){}

  _debug->println(F("Disconnect net"));
  net.DisConnect();

  _debug->println(F("Set APN and Password"));
  net.Configure(_apn, _user, _pass);

  _debug->println(F("Connect net"));
  net.Connect();

  _debug->println(F("Show My IP"));
  _debug->println(net.GetIP());*/
}