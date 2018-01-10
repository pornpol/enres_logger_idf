#ifndef ENRES3G_h
#define ENRES3G_h

/* _____STANDARD INCLUDES____________________________________________________ */
// include types & constants of Wiring core API
#include "Arduino.h"

#include <driver/uart.h>

#include "TEE_UC20.h"
#include "internet.h"
#include "File.h"
#include "http.h"

class ENRES3G
{
  public:
    ENRES3G();

    void begin(Stream &serial, Stream &debug, String, String, String);
    uint16_t post(String, String);
    
  private:
    Stream* _serial;
    Stream* _debug;
    String _apn;
    String _user;
    String _pass;

    INTERNET net;
    UC_FILE file;
    HTTP http;

    //void read_file(String pattern,String file_name);
    void debug(String data);

};

#endif