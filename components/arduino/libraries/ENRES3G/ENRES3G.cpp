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
  Serial.println(F("UC20"));
  gsm.begin(_serial);
  
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

  post("http://staging.enres.co/api/data_sensor/1001?access_token=8I3LaxaL8Yx5FASA4UpVh5I2swRoEO",
        "[{\"sdt\":\"201801081536\",\"rt\":24.39,\"hm\":50}]");
}

uint16_t ENRES3G::post(String url, String payload)
{
  Serial.println(F("Start HTTP"));
  http.begin(1);
  Serial.println(F("Send HTTP POST"));
  http.url(url);
  return(http.post(payload));
}