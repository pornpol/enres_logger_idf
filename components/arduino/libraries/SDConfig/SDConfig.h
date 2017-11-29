#ifndef SDConfig_h
#define SDConfig_h

/* _____STANDARD INCLUDES____________________________________________________ */
// include types & constants of Wiring core API
#include "Arduino.h"
#include "SD.h"
#include "FS.h"

#include "ArduinoJson.h"

#define MAXSSID   5

class SDConfig
{
  public:
    SDConfig();
    bool begin(uint8_t cs, Stream& debug);
    bool isConfigFileValid(const char * path);
    String readConfig(const char * path);
    bool readConfig(String config);
    //String readConfigString(const char * path);

    typedef struct __configGlobal
    {
      uint8_t type;
      uint16_t interval;
      uint8_t numMeter;
      uint8_t adcExt;
      String wifiSsid[MAXSSID];
      String wifiPass[MAXSSID];
      String url;
      String token;
      uint8_t batch;
      String path;
      String batch_path;
      String path_sensor;
      String batch_path_sensor;
    } configGlobal;
    configGlobal cfgG;

    typedef struct __configMeter
    {
      uint8_t id;
      uint32_t xid;
      uint8_t type;
      uint8_t index;
      float adjust[10];
    } configMeter;
    configMeter cfgM[21];

    typedef struct __configSensor
    {
      uint8_t used[2];
      uint32_t xid[2];
      uint8_t pin[8];
      uint8_t type[8];
      float adjust[8][8];
    } configSensor;
    configSensor cfgS;

    typedef struct __configFlow
    {
      uint8_t id;
      uint32_t xid;
      float adjust[9];
    } configFlow;
    configFlow cfgF;

  private:
    bool phaseConfig(String config);
    Stream* _debug;
    uint8_t _cs;
};

#endif