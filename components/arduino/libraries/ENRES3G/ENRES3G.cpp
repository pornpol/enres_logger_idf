#include "ENRES3G.h"
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>

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
  _debug->println(F("UC20"));
  gsm.begin(_serial);
  // gsm.Event_debug = debug;
  
  gsm.SetPowerKeyPin(13);
  
  esp_task_wdt_feed();
  gsm.PowerOn(); 

  esp_task_wdt_feed();
  while(gsm.WaitReady()){}

  _debug->println(F("Disconnect net"));
  esp_task_wdt_feed();
  net.DisConnect();

  _debug->println(F("Set APN and Password"));
  net.Configure(_apn, _user, _pass);

  _debug->println(F("Connect net"));
  esp_task_wdt_feed();
  net.Connect();

  _debug->println(F("Show My IP"));
  _debug->println(net.GetIP());
}

uint16_t ENRES3G::post(String url, String payload)
{
  _debug->println(F("Start HTTP"));
  http.begin(1);
  _debug->println(F("Send HTTP POST"));
  http.url(url);
  return(http.post(payload));
}

// void ENRES3G::debug(String data)
// {
//   _debug->println(data);
// }