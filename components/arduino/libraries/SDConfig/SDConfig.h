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
      uint16_t interval;
      uint8_t numMeter;
      String wifiSsid[MAXSSID];
      String wifiPass[MAXSSID];
      String url;
      String path;
      String token;
      uint8_t batch;
      String batch_path;
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

  private:
    Stream* _debug;
    uint8_t _cs;
};

#endif