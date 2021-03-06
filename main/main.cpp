#include <RTClib.h>
#include <ModbusMeter.h>
#include <ModbusFlow.h>
#include <SDConfig.h>
#include <SPIFlashMeter.h>
#include <AnalogSensor.h>
#include <HardwareWDT.h>
#include <NTPClient.h>
// #include <ENRES3G.h>

#include <time.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <driver/periph_ctrl.h>
#include <driver/uart.h>
#include <soc/uart_struct.h>

// Run this command after make clean
// rm ${IDF_PATH}/tools/kconfig/lxdialog/*.d

#define BUF_SIZE (1024)

#define MAX485_DE_RE      27
#define SD_CS             15
#define LED_BUILTIN       2
#define LED_3G            13
#define HWDT_KD           12
#define HWDT_EN           14

#define GMT               7

#define ADC_INT           0
#define ADC_EXT           1

#define RECSIZE           64  // 40 bytes >> 64
#define MAXMETER          21  //
#define RECSIZESENSOR     64  // 2 Group Hot & Cold

#define MAXPOST           6   // Max Single Post count per Loop

#define POSTSAVETIME      10  // (Sec) Time to prevent to POST loop before next read meter loop 

#define UNDERLINE         Serial.println("------------------------")

#define WIFIRETRY         5000 // Retry every 5 second

#define SRAM_ADDRESS      MCP7940_NVRAM

#define SERVER_CHK_INT    ((15)*60000) // millisecond (15 Minute)

#define RST_CHK_INT    ((1)*(24)*(3600000)) // millisecond (1 Day)

#define SW_VERSION        "0.98"

typedef union
{
  struct
  {
    uint16_t restart : 1;   // bit0
    uint16_t power : 1;     // bit1
    uint16_t network : 1;   // bit2
    uint16_t sdcard : 1;    // bit3
    uint16_t spiflash : 1;  // bit4
    uint16_t sensor : 1;    // bit5
    uint16_t server : 1;    // bit6
    uint16_t wifi : 1;      // bit7
    uint16_t bit8 : 1;
    uint16_t bit9 : 1;
    uint16_t bit10 : 1;
    uint16_t bit11 : 1;
    uint16_t bit12 : 1;
    uint16_t bit13 : 1;
    uint16_t bit14 : 1;
    uint16_t bit15 : 1;
  }e;
  uint16_t status;
}DeviceStatus;

DeviceStatus errlog;

// Define HardwareSerial(2) for Modbus Communication
HardwareSerial Serial2(2);
// HardwareSerial Serial1(1);

ModbusMeter meter;
SDConfig sd;
ModbusFlow flow;
SPIFlashMeter flash;
RTC_MCP7940 rtc;
AnalogSensor sensor;
HardwareWDT hwdt;
// ENRES3G enres3g;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", GMT*3600);

uint8_t wBuff[RECSIZE*MAXMETER];  // Max Buffer = Record Size * Max no. Meter
uint8_t rBuff[RECSIZE*MAXMETER];

// Should change to HW RTC
uint32_t wIndex = 0; 
uint32_t rIndex = 0;

struct tm timeinfo;     // Time Variable
time_t lastTime;        // Last read meter time
time_t lastUpdateRTC;   // Last Update RTC  
DateTime now;
uint64_t lastchkInRTC;
time_t lastTimeInRTC;

WiFiMulti wifiMulti;

uint8_t connType = 1;

bool tAdj = false;

uint8_t fErrorCnt = 0;
#define FLASH_ERROR_MAX 3
uint8_t fSecErrorCnt = 0;
#define FLASH_SECTOR_ERROR_MAX 3

uint32_t serverMillisChk = 0;
uint32_t rstMillisChk = 0;

uint64_t espChipID = 0;
String espChipID_str;

void preTransmission()
{
  digitalWrite(MAX485_DE_RE, HIGH);
}

void postTransmission()
{
  digitalWrite(MAX485_DE_RE, LOW);
}

void getWIndex()
{
  rtc.readnvram((uint8_t*)&wIndex, 4, SRAM_ADDRESS+1);
}

void setWIndex()
{
  rtc.writenvram(SRAM_ADDRESS+1, (uint8_t*)&wIndex, 4);
}

void getRIndex()
{
  rtc.readnvram((uint8_t*)&rIndex, 4, SRAM_ADDRESS+5);
}

void setRIndex()
{
  rtc.writenvram(SRAM_ADDRESS+5, (uint8_t*)&rIndex, 4);
}

// If use HW RTC must update this function
void wifiConnect()
{
  //uint8_t wifiCnt = 10; // 5 Seccond
    //connecting to a WiFi network
  Serial.println("Connecting to WiFi Network...");

  for(uint8_t i=0; i<MAXSSID; i++)
  {
    wifiMulti.addAP(const_cast<char*>(sd.cfgG.wifiSsid[i].c_str()), const_cast<char*>(sd.cfgG.wifiPass[i].c_str()));
  }

  // Update to have time out to exit loop
  if (wifiMulti.run() == WL_CONNECTED) {
    //Set LED to show wifi Status
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    UNDERLINE;
  }
}

/////////////////////////////////////// Meter ////////////////////////////////////

bool meterToFlash()
{
  uint16_t index = 0;
  // mdt, w, wh, pf, varh, i0, i1, 12, v0, v1, v2
  for(int i=0; i<sd.cfgG.numMeter; i++)
  {
    *(uint32_t*)&wBuff[index] = sd.cfgM[i].xid;   //[0, 4,294,967,295]
    index += 4;
    *(uint32_t*)&wBuff[index] = meter.md[i].mdt;  //[0, 4,294,967,295]
    index += 4;
 
    *(float*)&wBuff[index] = meter.md[i].watt;
    index += 4;
    *(float*)&wBuff[index] = meter.md[i].wattHour;
    index += 4;
    *(float*)&wBuff[index] = meter.md[i].pf;
    index += 4;
    *(float*)&wBuff[index] = meter.md[i].varh;
    index += 4;
    *(float*)&wBuff[index] = meter.md[i].i0;
    index += 4;
    *(float*)&wBuff[index] = meter.md[i].i1;
    index += 4;
    *(float*)&wBuff[index] = meter.md[i].i2;
    index += 4;
    *(float*)&wBuff[index] = meter.md[i].v0;
    index += 4;
    *(float*)&wBuff[index] = meter.md[i].v1;
    index += 4;
    *(float*)&wBuff[index] = meter.md[i].v2;
    index += 4;
    
    for(int i = index; (i%RECSIZE)>0; i++) //Zero padding
    {  
        wBuff[index] = 0;
        index++;
    }
  }
  
  Serial.printf("Write Index : %d\r\n", wIndex);

  if(flash.writeMeterData(wIndex, wBuff, RECSIZE*sd.cfgG.numMeter))
  {
    fErrorCnt = 0;
    Serial.println("Flash Write OK");
    wIndex += sd.cfgG.numMeter;
    // Save to RTC
    setWIndex();
    Serial.printf("Next Write Index : %d\r\n", wIndex);
    UNDERLINE;
    errlog.e.spiflash = 0;
    fErrorCnt = 0;
  }
  else 
  {
    Serial.println("Flash Write Not OK!!!!");
    UNDERLINE;
    errlog.e.spiflash = 1;

    fErrorCnt++;
    if((fErrorCnt >= FLASH_ERROR_MAX) && (wIndex == rIndex))
    {
      Serial.println("Move Flash Index to next sector!!!!");
      fErrorCnt = 0;
      wIndex = ((wIndex/64)+1)*64;
      rIndex = wIndex;
      setRIndex();
      setWIndex();
      //fSecErrorCnt++;
    }
    //return false;

  }
  
  return true;
}

void flashMeterToPost(uint32_t num)
{
  uint16_t index = 0;

  Serial.printf("Read Index : %d\r\n", rIndex);
  if(flash.readMeterData(rIndex, rBuff, RECSIZE*num))
  {
    Serial.println("Read Flash OK");
    UNDERLINE;
  }
  else
  {
    Serial.println("Read Flash Error!!!");
    UNDERLINE;
    return;
  }

  for(int i=0; i<num; i++)
  {
    String playload = "[{";
    String path = sd.cfgG.path;
    String host = sd.cfgG.url;

    path = path + String(*(uint32_t*)&rBuff[index]) + "?access_token=" + sd.cfgG.token;
    index += 4;

    char tbuff[64];
    struct tm *time_tm;
    time_t time_unix = *(uint32_t*)&rBuff[index];
    time_tm = localtime(&time_unix);
    strftime(tbuff, 64, "%Y%m%d%H%M", time_tm);
    
    playload = playload + "\"mdt\":\"" + String(tbuff) + "\",";
    index += 4;

    playload = playload + "\"w\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"wh\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"pf\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"varh\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"i0\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"i1\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"i2\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"v0\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"v1\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"v2\":" + String(((*(float*)&rBuff[index])), 4) + "}]";
    index += 4;

    for(int i = index; (i%RECSIZE)>0; i++)
    {
      index++;
    }

    Serial.print("HOST: ");
    Serial.println(host);

    Serial.print("PATH: ");
    Serial.println(path);

    //Serial.print("PL: ");
    //Serial.println(playload);

    HTTPClient http;

    http.begin("http://" + host + path);
    http.addHeader("Content-Type", "application/json");

    hwdt.disable();
    uint16_t httpCode = http.POST(playload);
    hwdt.enable();
    if (httpCode != 200) {
      Serial.println("ENRES Error code: " + String(httpCode) + " ros : " + http.getString());
      errlog.e.server = 1;

      // NTF
      WiFi.reconnect();

      return;
    } else
    {
      Serial.println("ENRES POST ok: " + String(httpCode) + " ros : " + http.getString());
      errlog.e.server = 0;
    }
    http.end();
    UNDERLINE;
    
  }

  rIndex += num;
  // Save to RTC
  setRIndex();
  Serial.printf("Next Read Index : %d\r\n", rIndex);
  UNDERLINE;
}

void flashMeterToBatchPost(uint32_t num)
{
  uint16_t index = 0;

  Serial.printf("Read Index : %d\r\n", rIndex);
  if(flash.readMeterData(rIndex, rBuff, RECSIZE*num))
  {
    Serial.println("Read Flash OK");
    UNDERLINE;
  }
  else
  {
    Serial.println("Read Flash Error!!!");
    UNDERLINE;
    return;
  }

  String playload = "[";
  String path = sd.cfgG.batch_path;
  String host = sd.cfgG.url;

  path = path + "?access_token=" + sd.cfgG.token;
  
  for(int i=0; i<num; i++)
  {
    playload = playload + "{\"emid\":\"" + String(*(uint32_t*)&rBuff[index]) + "\",";
    index += 4;

    char tbuff[64];
    struct tm *time_tm;
    time_t time_unix = *(uint32_t*)&rBuff[index];
    time_tm = localtime(&time_unix);
    strftime(tbuff, 64, "%Y%m%d%H%M", time_tm);
    
    playload = playload + "\"mdt\":\"" + String(tbuff) + "\",";
    index += 4;

    playload = playload + "\"w\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"wh\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"pf\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"varh\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"i0\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"i1\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"i2\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"v0\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"v1\":" + String(((*(float*)&rBuff[index])), 4) + ",";
    index += 4;

    playload = playload + "\"v2\":" + String(((*(float*)&rBuff[index])), 4) + "}";
    index += 4;

    if(i+1<num) playload = playload + ",";

    for(int i = index; (i%RECSIZE)>0; i++)
    {
      index++;
    }
  }

  playload = playload + "]";

  Serial.print("HOST: ");
  Serial.println(host);

  Serial.print("PATH: ");
  Serial.println(path);

  // Serial.print("PL: ");
  // Serial.println(playload);

  HTTPClient http;

  http.begin("http://" + host + path);
  http.addHeader("Content-Type", "application/json");

  hwdt.disable();
  uint16_t httpCode = http.POST(playload);
  hwdt.enable();

  if (httpCode != 200) {
    Serial.println("ENRES Error code: " + String(httpCode) + " ros : " + http.getString());
    errlog.e.server = 1;

    // NTF
    WiFi.reconnect();

    return;
  } else
  {
    Serial.println("ENRES POST ok: " + String(httpCode) + " ros : " + http.getString());
    errlog.e.server = 0;
  }
  http.end();
  UNDERLINE;
  
  rIndex += num;
  // Save to RTC
  setRIndex();
  Serial.printf("Next Read Index : %d\r\n", rIndex);
  UNDERLINE;
}

void ReadMeterToFlash_Task()
{
  /////////////// Energy Meter /////////////////
  for(uint8_t i = 0; i<sd.cfgG.numMeter; i++)
  {
    if(!meter.readMeterData(i, sd.cfgM[i].id, sd.cfgM[i].index, sd.cfgM[i].type, mktime(&timeinfo), sd.cfgM[i].adjust, sd.cfgM[i].table))
    {
      Serial.printf("Read Meter ID:%d OK\r\n", sd.cfgM[i].id);
      errlog.e.sensor = 0;
    } 
    else
    {
      Serial.printf("Read Meter ID:%d Error\r\n", sd.cfgM[i].id);
      errlog.e.sensor = 1;
      return;
    }
    esp_task_wdt_feed();
    hwdt.kickDog();

    delay(50); // Must add delay when switch slave ID
  }
  UNDERLINE;

  // Write data to flash << Must Check Data already read and update
  if(meterToFlash()) 
    lastTime = mktime(&timeinfo);
}

void ReadFlashMeterToPost_Task()
{
  static uint32_t disCnt = 0;
  
  hwdt.disable();
  if(wifiMulti.run() == WL_CONNECTED)
  {
    hwdt.enable();
    // Set LED to show WiFi Status
    digitalWrite(LED_BUILTIN, LOW);
    // Has Data to POST
    if(rIndex < wIndex)
    {
      uint32_t numPost = wIndex - rIndex;

      if(sd.cfgG.batch == 0)
      {
        if(numPost > MAXPOST) numPost = MAXPOST; // Set Max POST per loop to prevent read meter 
        Serial.printf("Number(s) to POST : %d\r\n", numPost);
        flashMeterToPost(numPost);
      }
      else
      {
        if(numPost > sd.cfgG.batch) numPost = sd.cfgG.batch;
        Serial.printf("Number(s) to POST : %d\r\n", numPost);
        flashMeterToBatchPost(numPost);
      }
    }
    disCnt = millis();
  }
  else
  {
    hwdt.enable();
    // Retry Every 5 Sec.
    if(millis()-disCnt > WIFIRETRY)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("WiFi Not Connecting");
      UNDERLINE;
      disCnt = millis();
      hwdt.kickDog();
    }
  }
}
//////////////////////////////////// End Meter /////////////////////////////

/////////////////////////////////// Sensor ////////////////////////////////

bool SensorToFlash()
{
  uint16_t index = 0;
  
  for(uint8_t i=0; i<2; i++)
  {
    *(uint8_t*)&wBuff[index] = sd.cfgS.used[i];
    index += 1;
    *(uint32_t*)&wBuff[index] = sd.cfgS.xid[i];   //[0, 4,294,967,295]
    index += 4;
    *(uint32_t*)&wBuff[index] = mktime(&timeinfo);
    index += 4;

    for(uint8_t j=0; j<4; j++)
    {
      float sValue;

      sValue = sensor.getSensor((4*i)+j) * sd.cfgS.adjust[(4*i)+j][0] + sd.cfgS.adjust[(4*i)+j][1];
      Serial.printf("Sensor %d : %f\r\n", (4*i)+j, sValue);

      *(float*)&wBuff[index]  = sValue;
      index += 4;
    }
  }
  for(int i = index; (i%RECSIZESENSOR)>0; i++) //Zero padding
  {  
    wBuff[index] = 0;
    index++;
  }

  Serial.printf("Write Sensor Index : %d\r\n", wIndex);
  
  if(flash.writeSensorData(wIndex, wBuff, RECSIZESENSOR))
  {
    Serial.println("Flash Write OK");
    wIndex += 1;
    // Save to RTC
    setWIndex();
    Serial.printf("Next Write Sensor Index : %d\r\n", wIndex);
    UNDERLINE;
    errlog.e.spiflash = 0;
    fErrorCnt = 0;
  }
  else 
  {
    Serial.println("Flash Write Not OK!!!!");
    UNDERLINE;
    errlog.e.spiflash = 1;
    
    fErrorCnt++;
    if((fErrorCnt >= FLASH_ERROR_MAX) && (wIndex == rIndex))
    {
      Serial.println("Move Flash Index to next sector!!!!");
      fErrorCnt = 0;
      wIndex = ((wIndex/64)+1)*64;
      rIndex = wIndex;
      setRIndex();
      setWIndex();
      //fSecErrorCnt++;
    }
    //return false;

  }
  
  return true;
}

void flashSensorToPost(uint32_t num)
{
}

void flashSensorToBatchPost(uint32_t num)
{
  uint16_t index = 0;
  
    Serial.printf("Read Index : %d\r\n", rIndex);
    if(flash.readSensorData(rIndex, rBuff, RECSIZESENSOR*num))
    {
      Serial.println("Read Flash OK");
      UNDERLINE;
    }
    else
    {
      Serial.println("Read Flash Error!!!");
      UNDERLINE;
      return;
    }
  
    uint8_t cnt = 0;

    String playload = "[";
    String path = sd.cfgG.batch_path_sensor;
    String host = sd.cfgG.url;
  
    path = path + "?access_token=" + sd.cfgG.token;
    
    for(uint8_t i=0; i<num; i++)
    {
      uint8_t cnt = 0;
      for(uint8_t j=0; j<2; j++)
      {
        uint8_t used_type = *(uint8_t*)&rBuff[index];
        if(used_type >= 1) // Change to >= 1
        {
          if(cnt > 0) playload = playload + ",";
          cnt++;

          index += 1;
          playload = playload + "{\"external_sensor_id\":" + String(*(uint32_t*)&rBuff[index]) + ",";
          index += 4;
      
          char tbuff[64];
          struct tm *time_tm;
          time_t time_unix = *(uint32_t*)&rBuff[index];
          time_tm = localtime(&time_unix);
          strftime(tbuff, 64, "%Y%m%d%H%M", time_tm);
          
          playload = playload + "\"sdt\":\"" + String(tbuff) + "\",";
          index += 4;
      
          if(used_type == 1) // Pressure Cooling
          {
            playload = playload + "\"tm1\":" + String(((*(float*)&rBuff[index])), 4) + ",";
            index += 4;
        
            playload = playload + "\"tm2\":" + String(((*(float*)&rBuff[index])), 4) + ",";
            index += 4;
        
            playload = playload + "\"ps1\":" + String(((*(float*)&rBuff[index])), 4) + ",";
            index += 4;
        
            playload = playload + "\"ps2\":" + String(((*(float*)&rBuff[index])), 4) + "}";
            index += 4;
          }else if(used_type == 2) // Temp Humid
          {
            playload = playload + "\"rt\":" + String(((*(float*)&rBuff[index])), 4) + ",";
            index += 4;
        
            playload = playload + "\"hm\":" + String(((*(float*)&rBuff[index])), 4) + "}";
            index += 12;
          }else if(used_type == 3) // CO2
          {
            playload = playload + "\"co2\":" + String(((*(float*)&rBuff[index])), 4) + "}";
            index += 16;
          }else if(used_type == 4) // Pressure
          {
            playload = playload + "\"ps\":" + String(((*(float*)&rBuff[index])), 4) + "}";
            index += 16;
          }
        }
        else
        {
          index += 25;
        }
      }

      if(i+1<num) playload = playload + ",";
  
      for(int i = index; (i%RECSIZESENSOR)>0; i++)
      {
        index++;
      }
    }
  
    playload = playload + "]";
  
    Serial.print("HOST: ");
    Serial.println(host);
  
    Serial.print("PATH: ");
    Serial.println(path);
  
    // Serial.print("PL: ");
    // Serial.println(playload);
  
    uint16_t httpCode;
    HTTPClient http;
  
    http.begin("http://" + host + path);
    http.addHeader("Content-Type", "application/json");
  
    hwdt.disable();
    httpCode = http.POST(playload);
    hwdt.enable();
    if (httpCode != 200) {
      Serial.println("ENRES Error code: " + String(httpCode) + " ros : " + http.getString());
      errlog.e.server = 1;

      // NTF
      WiFi.reconnect();

      return;
    } else
    {
      Serial.println("ENRES POST ok: " + String(httpCode) + " ros : " + http.getString());
      errlog.e.server = 0;
    }
    http.end();
    UNDERLINE;

    rIndex += num;
    // Save to RTC
    setRIndex();
    Serial.printf("Next Read Index : %d\r\n", rIndex);
    UNDERLINE;
}

void ReadSensorToFlash_Task()
{
  // Write data to flash << Must Check Data already read and update
  if(SensorToFlash())
    lastTime = mktime(&timeinfo);
}

void ReadFlashSensorToPost_Task()
{
  static uint32_t disCnt = 0;
  
  hwdt.disable();
  if(wifiMulti.run() == WL_CONNECTED)
  {
    hwdt.enable();
    // Set LED to show WiFi Status
    digitalWrite(LED_BUILTIN, LOW);
    // Has Data to POST
    if(rIndex < wIndex)
    {
      uint32_t numPost = wIndex - rIndex;

      if(sd.cfgG.batch == 0)
      {
        if(numPost > MAXPOST) numPost = MAXPOST; // Set Max POST per loop to prevent read meter 
        Serial.printf("Number(s) to POST : %d\r\n", numPost);
        flashSensorToPost(numPost);
      }
      else
      {
        if(numPost > sd.cfgG.batch) numPost = sd.cfgG.batch;
        Serial.printf("Number(s) to POST : %d\r\n", numPost);
        flashSensorToBatchPost(numPost);
      }
    }
    disCnt = millis();
  }
  else
  {
    hwdt.enable();
    // Retry Every 5 Sec.
    if(millis()-disCnt > WIFIRETRY)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("WiFi Not Connecting");
      UNDERLINE;
      disCnt = millis();
      hwdt.kickDog();
    }
  }
}
/////////////////////////////////// End Sensor /////////////////////////////

//////////////////////////////////// Flow //////////////////////////////////
bool flowToFlash()
{
  uint16_t index = 0;
  
  // inst_flux ins_heat_flow tm1 tm2 signal_ratio reynolds q_str up_str down_str
  *(uint32_t*)&wBuff[index] = sd.cfgF.xid;   //[0, 4,294,967,295]
  index += 4;
  *(uint32_t*)&wBuff[index] = flow.fd.mdt;  //[0, 4,294,967,295]
  index += 4;
  *(float*)&wBuff[index] = flow.fd.inst_flux;
  index += 4;
  *(float*)&wBuff[index] = flow.fd.ins_heat_flow;
  index += 4;

  // 
  // *(float*)&wBuff[index] = sensor.getSensor(0) * sd.cfgS.adjust[0][0] + sd.cfgS.adjust[0][1];
  *(float*)&wBuff[index] = flow.fd.tm1;
  index += 4;
  // *(float*)&wBuff[index] = sensor.getSensor(1) * sd.cfgS.adjust[1][0] + sd.cfgS.adjust[1][1];
  *(float*)&wBuff[index] = flow.fd.tm2;
  index += 4;

  *(float*)&wBuff[index] = flow.fd.signal_ratio;
  index += 4;
  *(float*)&wBuff[index] = flow.fd.reynolds;
  index += 4;
  *(float*)&wBuff[index] = flow.fd.q_str;
  index += 4;
  *(float*)&wBuff[index] = flow.fd.up_str;
  index += 4;
  *(float*)&wBuff[index] = flow.fd.down_str;
  index += 4;

  for(int i = index; (i%RECSIZE)>0; i++) //Zero padding
  {  
      wBuff[index] = 0;
      index++;
  }
  
  Serial.printf("Write Index : %d\r\n", wIndex);

  if(flash.writeFlowData(wIndex, wBuff, RECSIZE))
  {
    Serial.println("Flash Write OK");
    wIndex += 1;
    // Save to RTC
    setWIndex();
    Serial.printf("Next Write Index : %d\r\n", wIndex);
    UNDERLINE;
    errlog.e.spiflash = 0;
    fErrorCnt = 0;
  }
  else 
  {
    Serial.println("Flash Write Not OK!!!!");
    UNDERLINE;
    errlog.e.spiflash = 1;
    
    fErrorCnt++;
    if((fErrorCnt >= FLASH_ERROR_MAX) && (wIndex == rIndex))
    {
      Serial.println("Move Flash Index to next sector!!!!");
      fErrorCnt = 0;
      wIndex = ((wIndex/64)+1)*64;
      rIndex = wIndex;
      setRIndex();
      setWIndex();
      //fSecErrorCnt++;
    }
    //return false;

  }
  
  return true;
}

void flashFlowToPost(uint32_t num)
{
}

void flashFlowToBatchPost(uint32_t num)
{
  uint16_t index = 0;
  
    Serial.printf("Read Index : %d\r\n", rIndex);
    if(flash.readFlowData(rIndex, rBuff, RECSIZE*num))
    {
      Serial.println("Read Flash OK");
      UNDERLINE;
    }
    else
    {
      Serial.println("Read Flash Error!!!");
      UNDERLINE;
      return;
    }
  
    String playload = "[";
    String path = sd.cfgG.batch_path_sensor;
    String host = sd.cfgG.url;
  
    path = path + "?access_token=" + sd.cfgG.token;
    
    for(int i=0; i<num; i++)
    {
      playload = playload + "{\"external_sensor_id\":\"" + String(*(uint32_t*)&rBuff[index]) + "\",";
      index += 4;
  
      char tbuff[64];
      struct tm *time_tm;
      time_t time_unix = *(uint32_t*)&rBuff[index];
      time_tm = localtime(&time_unix);
      strftime(tbuff, 64, "%Y%m%d%H%M", time_tm);
      
      playload = playload + "\"sdt\":\"" + String(tbuff) + "\",";
      index += 4;
  
      playload = playload + "\"vd1\":" + String(((*(float*)&rBuff[index])), 4) + ",";
      index += 4;
  
      playload = playload + "\"vd2\":" + String(((*(float*)&rBuff[index])), 4) + ",";
      index += 4;
  
      playload = playload + "\"vd3\":" + String(((*(float*)&rBuff[index])), 4) + ",";
      index += 4;
  
      playload = playload + "\"vd4\":" + String(((*(float*)&rBuff[index])), 4) + ",";
      index += 4;
  
      playload = playload + "\"vd5\":" + String(((*(float*)&rBuff[index])), 4) + ",";
      index += 4;
  
      playload = playload + "\"vd6\":" + String(((*(float*)&rBuff[index])), 4) + ",";
      index += 4;
  
      playload = playload + "\"vd7\":" + String(((*(float*)&rBuff[index])), 4) + ",";
      index += 4;
  
      playload = playload + "\"vd8\":" + String(((*(float*)&rBuff[index])), 4) + ",";
      index += 4;
  
      playload = playload + "\"vd9\":" + String(((*(float*)&rBuff[index])), 4) + "}";
      index += 4;
  
      if(i+1<num) playload = playload + ",";
  
      for(int i = index; (i%RECSIZE)>0; i++)
      {
        index++;
      }
    }
  
    playload = playload + "]";
  
    Serial.print("HOST: ");
    Serial.println(host);
  
    Serial.print("PATH: ");
    Serial.println(path);
  
    //Serial.print("PL: ");
    //Serial.println(playload);
  
    HTTPClient http;
  
    http.begin("http://" + host + path);
    http.addHeader("Content-Type", "application/json");
  
    hwdt.disable();
    uint16_t httpCode = http.POST(playload);
    hwdt.enable();
  
    if (httpCode != 200) {
      Serial.println("ENRES Error code: " + String(httpCode) + " ros : " + http.getString());
      errlog.e.server = 1;

      // NTF
      WiFi.reconnect();
      
      return;
    } else
    {
      Serial.println("ENRES POST ok: " + String(httpCode) + " ros : " + http.getString());
      errlog.e.server = 0;
    }
    http.end();
    UNDERLINE;
    
    rIndex += num;
    // Save to RTC
    setRIndex();
    Serial.printf("Next Read Index : %d\r\n", rIndex);
    UNDERLINE;
}

void ReadFlowToFlash_Task()
{
  if(!flow.readFlowData(sd.cfgF.id, mktime(&timeinfo), sd.cfgF.adjust))
  {
    Serial.printf("Read Flow ID:%d OK\r\n", sd.cfgF.id);
  } 
  else
  {
    Serial.printf("Read Flow ID:%d Error\r\n", sd.cfgF.id);
    return;
  }
  esp_task_wdt_feed();
  hwdt.kickDog();

  UNDERLINE;

  // Write data to flash << Must Check Data already read and update
  if(flowToFlash()) 
    lastTime = mktime(&timeinfo);  
}

void ReadFlashFlowToPost_Task()
{
  static uint32_t disCnt = 0;
  
  hwdt.disable();
  if(wifiMulti.run() == WL_CONNECTED)
  {
    hwdt.enable();
    // Set LED to show WiFi Status
    digitalWrite(LED_BUILTIN, LOW);
    // Has Data to POST
    if(rIndex < wIndex)
    {
      uint32_t numPost = wIndex - rIndex;

      if(sd.cfgG.batch == 0)
      {
        if(numPost > MAXPOST) numPost = MAXPOST; // Set Max POST per loop to prevent read meter 
        Serial.printf("Number(s) to POST : %d\r\n", numPost);
        flashFlowToPost(numPost);
      }
      else
      {
        if(numPost > sd.cfgG.batch) numPost = sd.cfgG.batch;
        Serial.printf("Number(s) to POST : %d\r\n", numPost);
        flashFlowToBatchPost(numPost);
      }
    }
    disCnt = millis();
  }
  else
  {
    hwdt.enable();
    // Retry Every 5 Sec.
    if(millis()-disCnt > WIFIRETRY)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("WiFi Not Connecting");
      UNDERLINE;
      disCnt = millis();
      hwdt.kickDog();
    }
  }
}
/////////////////////////////////// End Flow //////////////////////////////

bool serverChkTask()
{
  Serial.println("Server Checking...");
  
  hwdt.disable();
  if(wifiMulti.run() == WL_CONNECTED)
  {
    hwdt.enable();
    HTTPClient http;
    
    http.begin("http://" + sd.cfgG.log_server + "/nreq/" + espChipID_str);
    
    hwdt.disable();
    uint16_t httpCode = http.GET();
    hwdt.enable();
    if (httpCode != 200) {
      Serial.println("ENRES Error code: " + String(httpCode));
      errlog.e.server = 1;
      return false;
    } else
    {
      Serial.println("ENRES GET REQ ok: " + String(httpCode));
      errlog.e.server = 0;
      DynamicJsonBuffer jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(http.getString());

      if(uint8_t(root["restart"]) == 1)
      {
        Serial.println("Request to Restart");
        delay(1000);
        ESP.restart();
      }
      if(uint8_t(root["restart"]) == 2)
      {
        wIndex = 0;
        rIndex = 0;
        setWIndex();
        setRIndex();

        Serial.println("Request to Reset Flash Index");
        delay(1000);
        ESP.restart();
      }
      return true;
    }
    http.end();
  }else
  {
    hwdt.enable();
    Serial.println("No Wifi Connection");
    return false;
  }
  return false;
}

bool logPostTask()
{
  // errlog.e.power = ~(digitalRead(PWR_CHK));

  Serial.println("Log Posting...");
  hwdt.disable();
  if(wifiMulti.run() == WL_CONNECTED)
  {
    hwdt.enable();
    HTTPClient http;

    char tbuff[64];
    strftime(tbuff, 64, "%Y%m%d%H%M", &timeinfo);

    String playload = "[{";
    playload = playload + "\"dt\":\"" + String(tbuff) + "\",";
    playload = playload + "\"msg\":\"" + String(errlog.status) + "\"";
    playload = playload + "}]";
    
    http.begin("http://" + sd.cfgG.log_server + "/log/" + espChipID_str);
    http.addHeader("Content-Type", "application/json");

    hwdt.disable();
    uint16_t httpCode = http.POST(playload);
    hwdt.enable();

    if (httpCode != 200) {
      Serial.println("ENRES Error code: " + String(httpCode) + " ros : " + http.getString());
      errlog.e.server = 1;
      return false;
    } else
    {
      Serial.println("ENRES POST ok: " + String(httpCode) + " ros : " + http.getString());
      errlog.status = 0;
    }
    http.end();
    return true;
  }else
  {
    hwdt.enable();
    Serial.println("No Wifi Connection");
    return false;    
  }

  return false;
}

void setup()
{
  esp_task_wdt_feed();

  // Get ESP Chip id
  char buff[12];
  espChipID = ESP.getEfuseMac();
  sprintf(buff, "%04X", (uint16_t)(espChipID>>32));
  sprintf(buff+4, "%08X", (uint32_t)espChipID);
  espChipID_str = String(buff);

  // Enable Hardware kickDog
  hwdt.begin(HWDT_KD, HWDT_EN);
  hwdt.kickDog();

  delay(100);
  pinMode(MAX485_DE_RE, OUTPUT);
  // Init in receive mode
  digitalWrite(MAX485_DE_RE, LOW);

  // Init WiFi LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(LED_3G, OUTPUT);
  digitalWrite(LED_3G, HIGH);

  // pinMode(PWR_CHK, INPUT);

  // Init debug port
  Serial.begin(115200, SERIAL_8N1);

  Serial.printf("ESP Chip ID : %s\r\n", espChipID_str.c_str());

  Serial.printf("Software Version : %s\r\n", SW_VERSION);

  // Init SD
  sd.begin(SD_CS, Serial);

  // Init Flash
  if(!flash.begin(RECSIZE, Serial)) 
  {
    delay(1000);
    ESP.restart();
  }
  UNDERLINE;

  // Init SD Configuration
  if(sd.isConfigFileValid("/config")) // SD Mount
    flash.writeConfigFlash(sd.readConfig("/config"));
  else // SD Unmount
    sd.readConfig(flash.readConfigFlash());

  UNDERLINE;

  // Init Modbus Meter Communication
  const uart_port_t uart_num_2 = UART_NUM_2;
  uart_config_t uart_config_2 = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_EVEN,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
  };
  if(sd.cfgG.type == 1)
    uart_config_2.parity = UART_PARITY_EVEN;
  else if(sd.cfgG.type == 3)
    uart_config_2.parity = UART_PARITY_DISABLE;

  if(sd.cfgG.mbc == 1)
    uart_config_2.parity = UART_PARITY_EVEN;
  else if(sd.cfgG.mbc == 2)
    uart_config_2.parity = UART_PARITY_DISABLE;
  
  uart_param_config(uart_num_2, &uart_config_2);
  uart_set_pin(uart_num_2, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(uart_num_2, BUF_SIZE * 2, 0, 0, NULL, 0);
  
  // Init 3G Module Communication
  // Serial1.begin(9600, SERIAL_8N1, 26 /*rx*/, 25 /*tx*/);

  uart_disable_rx_intr(UART_NUM_0);

  meter.begin(Serial2);
  meter.preTransmission(preTransmission);
  meter.postTransmission(postTransmission);

  flow.begin(Serial2);
  flow.preTransmission(preTransmission);
  flow.postTransmission(postTransmission);

  // Init Sensor
  sensor.begin(sd.cfgS.pin, sd.cfgS.type, sd.cfgG.adcExt, sd.cfgS.range);

  // Init Rtc
  // Check RTC RAM and update wIndex & rIndex
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }

  // Test Backup Index
  if(rtc.readnvram(SRAM_ADDRESS) != 0x55)
  {
    Serial.println(rtc.readnvram(SRAM_ADDRESS));
    Serial.println("Init NVRAM Backup");
    rtc.writenvram(SRAM_ADDRESS, 0x55);

    setWIndex();
    setRIndex();
  }
  else
  {
    getWIndex();
    getRIndex();
  }

  // Connect to Wifi
  hwdt.disable();
  wifiConnect();
  hwdt.enable();

  hwdt.kickDog();

  now = rtc.now();
  //configTime((-1)*GMT*3600, 0, "pool.ntp.org");
  timeClient.begin();
  
  hwdt.disable();
  delay(1000);
  hwdt.enable();

  // loop while NTP not sync or rtc not set
  Serial.println("Wait for time sync");
  Serial.printf("Year : %d\r\n", now.year());
  while(!(timeClient.update() || (now.year() >= 2017)))
  {
    esp_task_wdt_feed();
    hwdt.disable();
    wifiMulti.run();
    hwdt.enable();
  }

  hwdt.kickDog();
  esp_task_wdt_feed();

  errlog.e.restart = 1;

}

void loop()
{
  esp_task_wdt_feed();
  hwdt.kickDog();

  if(timeClient.update())
  {
    time_t ntpTime = timeClient.getEpochTime();
    timeinfo = *(localtime(&ntpTime));

    // Set to HWRTC check mon and year
    if(tAdj == false)
    {
      Serial.println(ntpTime);
      Serial.printf("Adjust Time : %d/%d/%d %d:%d:%d\r\n", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      rtc.adjust(DateTime(timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      lastUpdateRTC = millis(); //mktime(&timeinfo);
      tAdj = true;
      lastTime = 0; // Read Meter after sync rtc
    }

    // Check Update RTC Every 60 Minute
    if(millis()-lastUpdateRTC >= (15*60000))
    {
      Serial.println(ntpTime);
      Serial.printf("Adjust Time : %d/%d/%d %d:%d:%d\r\n", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      rtc.adjust(DateTime(timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      lastUpdateRTC =  millis(); //mktime(&timeinfo);
    }
  }
  else Serial.println("Time Update Error");
  // else
  // {
    now = rtc.now();

    timeinfo.tm_year  = now.year()-1900;
    timeinfo.tm_mon   = now.month()-1; 
    timeinfo.tm_mday  = now.day(); 
    timeinfo.tm_hour  = now.hour();
    timeinfo.tm_min   = now.minute();
    timeinfo.tm_sec   = now.second();
  // }

  // Fix Fault time at start
  if(lastTime > mktime(&timeinfo))
  {
    lastTime = mktime(&timeinfo);
    Serial.printf("Time Error\r\n");
  }

  esp_task_wdt_feed();
  hwdt.kickDog();

  // Read Data to Flash
  if(mktime(&timeinfo)-lastTime >= sd.cfgG.interval)
  {
    Serial.println(&timeinfo);
    UNDERLINE;

    if(sd.cfgG.type == 1)
      ReadMeterToFlash_Task();
    else if(sd.cfgG.type == 2)
      ReadSensorToFlash_Task();
    else if(sd.cfgG.type == 3)
      ReadFlowToFlash_Task();
  }
  
  esp_task_wdt_feed();
  hwdt.kickDog();

  // Read Flash to Post
  if(mktime(&timeinfo)-lastTime < sd.cfgG.interval-POSTSAVETIME)
  {
    if(sd.cfgG.type == 1)
      ReadFlashMeterToPost_Task();
    else if(sd.cfgG.type == 2)
      ReadFlashSensorToPost_Task();
    else if(sd.cfgG.type == 3)
      ReadFlashFlowToPost_Task();  
  }

  esp_task_wdt_feed();
  hwdt.kickDog();

  // Server Chk
  if(millis()-serverMillisChk > SERVER_CHK_INT)
  {
    serverMillisChk = millis();
    if(sd.cfgG.log_use == 1)
    {
      if(serverChkTask())
        logPostTask();
    }else
    {
      Serial.println("No log Server Config");
    }
  }
  
  // Interval Reset Chk
  if(millis()-rstMillisChk > RST_CHK_INT)
  {
    rstMillisChk = millis();
    Serial.println("Device Routine Resetting...");
    delay(1000);
    ESP.restart();
  }  
}