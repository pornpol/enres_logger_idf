#include "SDConfig.h"

SDConfig::SDConfig()
{
}

bool SDConfig::begin(uint8_t cs, Stream &debug)
{
  _cs = cs;
  _debug = &debug;

  if(!SD.begin(_cs)){
    _debug->println("Card Mount Failed");
    return false;
  }else _debug->println("Init SD Card OK");

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  return true;
}

bool SDConfig::isConfigFileValid(const char * path)
{
  File file = SD.open(path);
  if(!file){
    _debug->println("Failed to open file for reading");
    return false;
  }
  return true;
}

String SDConfig::readConfig(const char * path)
{
  String config;

  _debug->printf("Reading file: %s\n", path);

  File file = SD.open(path);
  if(!file){
    _debug->println("Failed to open file for reading");
    return "";
  }

  //Read config.json to String
  while(file.available()){
      config += char(file.read());
  }
  file.close();

  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(config);

  cfgG.interval = root["global"]["interval"];
  cfgG.numMeter = root["global"]["meters"];
  for(uint8_t i=0; i<MAXSSID; i++)
  {
    cfgG.wifiSsid[i] = root["global"]["wifi"][i]["ssid"].as<String>();
    cfgG.wifiPass[i] = root["global"]["wifi"][i]["pass"].as<String>();
  }
  cfgG.url      = root["global"]["url"].as<String>();
  cfgG.path      = root["global"]["path"].as<String>();
  cfgG.token    = root["global"]["token"].as<String>();
  cfgG.batch    = root["global"]["batch"];
  cfgG.batch_path    = root["global"]["batch_path"].as<String>();

  for(uint8_t i=0; i<cfgG.numMeter; i++)
  {
    cfgM[i].id    = root["meter"][i]["id"];
    cfgM[i].xid   = root["meter"][i]["xid"];
    cfgM[i].type  = root["meter"][i]["type"];
    cfgM[i].index  = root["meter"][i]["index"];
    for(uint8_t j=0; j<10; j++)
    {
      cfgM[i].adjust[j] = root["meter"][i]["adjust"][j];
    } 
  }

  return config;
}

bool SDConfig::readConfig(String config)
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(config);

  cfgG.interval = root["global"]["interval"];
  cfgG.numMeter = root["global"]["meters"];
  for(uint8_t i=0; i<MAXSSID; i++)
  {
    cfgG.wifiSsid[i] = root["global"]["wifi"][i]["ssid"].as<String>();
    cfgG.wifiPass[i] = root["global"]["wifi"][i]["pass"].as<String>();
  }
  cfgG.url      = root["global"]["url"].as<String>();
  cfgG.path      = root["global"]["path"].as<String>();
  cfgG.token    = root["global"]["token"].as<String>();
  cfgG.batch    = root["global"]["batch"];
  cfgG.batch_path    = root["global"]["batch_path"].as<String>();

  for(uint8_t i=0; i<cfgG.numMeter; i++)
  {
    cfgM[i].id    = root["meter"][i]["id"];
    cfgM[i].xid   = root["meter"][i]["xid"];
    cfgM[i].type  = root["meter"][i]["type"];
    cfgM[i].index  = root["meter"][i]["index"];
    for(uint8_t j=0; j<10; j++)
    {
      cfgM[i].adjust[j] = root["meter"][i]["adjust"][j];
    } 
  }

  return true;
}

// String SDConfig::readConfigString(const char * path)
// {
//   String config;
  
//   _debug->printf("Reading file: %s\n", path);

//   File file = SD.open(path);
//   if(!file){
//     _debug->println("Failed to open file for reading");
//     return "";
//   }

//   //Read config.json to String
//   while(file.available()){
//       config += char(file.read());
//   }
//   file.close();

//   return config;
// }