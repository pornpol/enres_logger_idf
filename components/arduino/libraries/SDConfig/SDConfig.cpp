#include "SDConfig.h"

SDConfig::SDConfig()
{
  // Add default Config
  cfgG.interval = 60;
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

  phaseConfig(config);

  return config;
}

bool SDConfig::readConfig(String config)
{
  return phaseConfig(config);
}

bool SDConfig::phaseConfig(String config)
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(config);

  cfgG.type = root["global"]["type"];
  cfgG.interval = root["global"]["interval"];
  cfgG.numMeter = root["global"]["meters"];
  cfgG.adcExt = root["global"]["adc_ext"];
  cfgG.mbc = root["global"]["mbc"];
  for(uint8_t i=0; i<MAXSSID; i++)
  {
    cfgG.wifiSsid[i] = root["global"]["wifi"][i]["ssid"].as<String>();
    cfgG.wifiPass[i] = root["global"]["wifi"][i]["pass"].as<String>();
  }
  cfgG.url      = root["global"]["url"].as<String>();
  cfgG.token    = root["global"]["token"].as<String>();
  cfgG.batch    = root["global"]["batch"];
  cfgG.path      = root["global"]["path"].as<String>();
  cfgG.batch_path    = root["global"]["batch_path"].as<String>();
  cfgG.path_sensor = root["global"]["path_sensor"].as<String>();
  cfgG.batch_path_sensor = root["global"]["batch_path_sensor"].as<String>();
  cfgG.log_use = root["global"]["log_use"];
  cfgG.log_server    = root["global"]["log_server"].as<String>();

  for(uint8_t i=0; i<cfgG.numMeter; i++)
  {
    cfgM[i].id    = root["meter"][i]["id"];
    cfgM[i].xid   = root["meter"][i]["xid"];
    cfgM[i].type  = root["meter"][i]["type"];
    cfgM[i].index  = root["meter"][i]["index"];
    for(uint8_t j=0; j<10; j++)
    {
      cfgM[i].adjust[j] = root["meter"][i]["adjust"][j];
      cfgM[i].table[j] = root["meter"][i]["table"][j];
    }
    cfgM[i].table[10] = root["meter"][i]["table"][10]; // Modbus Function
  }

  for(uint8_t i=0; i<2; i++)
  {
    cfgS.used[i]  = root["sensor"]["used"][i];
    cfgS.xid[i]   = root["sensor"]["xid"][i];   
  }

  for(uint8_t i=0; i<8; i++)
  {
    cfgS.pin[i] = root["sensor"]["pin"][i];
    cfgS.type[i] = root["sensor"]["type"][i];
    for(uint8_t j=0; j<2; j++)
    {
      cfgS.range[i][j] = root["sensor"]["range"][i][j];
      cfgS.adjust[i][j] = root["sensor"]["adjust"][i][j];
    }
  }

  cfgF.id = root["flow"]["id"];
  cfgF.xid = root["flow"]["xid"];
  for(uint8_t i=0; i<9; i++)
  {
    cfgF.adjust[i] = root["flow"]["adjust"][i];
  } 

  return true;
}