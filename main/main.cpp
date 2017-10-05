//#include <Wire.h>
#include <RTClib.h>
#include <ModbusMeter.h>
#include <SDConfig.h>
#include <SPIFlashMeter.h>

#include <time.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <driver/periph_ctrl.h>
#include <driver/uart.h>
#include <soc/uart_struct.h>

#define BUF_SIZE (1024)

#define MAX485_DE_RE      27
#define SD_CS             15
#define LED_BUILTIN       2
//#define FLASH_CS          5
#define FLASH_SIZE        16  //Mb
#define GMT               7
#define RECSIZE           64  // 40 bytes >> 64
#define MAXMETER          21  //

#define MAXPOST           6   // Max Single Post count per Loop

#define POSTSAVETIME      10  // Time to prevent to POST loop before next read meter loop

#define UNDERLINE         Serial.println("------------------------")

#define WIFIRETRY         5000 // Retry every 5 second

#define SRAM_ADDRESS      MCP7940_NVRAM

// Define HardwareSerial(2) for Modbus Communication
HardwareSerial Serial2(2);

ModbusMeter meter;
SDConfig sd;
SPIFlashMeter flash;
RTC_MCP7940 rtc;

uint8_t wBuff[RECSIZE*MAXMETER];  // Max Buffer = Record Size * Max no. Meter
uint8_t rBuff[RECSIZE*MAXMETER];

// Should change to HW RTC
uint32_t wIndex;     // Move to RTC to prevent be cleared after reset 
uint32_t rIndex;     // Move to RTC to prevent be cleared after reset 

struct tm timeinfo;     // Time Variable
time_t lastTime;        // Last read meter time
time_t lastUpdateRTC;   // Last Update RTC
DateTime now;

WiFiMulti wifiMulti;

bool tAdj = false;

uint8_t fErrorCnt = 0;
#define FLASH_ERROR_MAX 5

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

bool dataToFlash()
{
  uint16_t index = 0;
  // mdt, w, wh, pf, varh, i0, i1, 12, v0, v1, v2
  for(int i=0; i<sd.cfgG.numMeter; i++)
  {
    *(uint32_t*)&wBuff[index] = sd.cfgM[i].xid;   //[0, 4,294,967,295]
    index += 4;
    *(uint32_t*)&wBuff[index] = meter.md[i].mdt;  //[0, 4,294,967,295]
    index += 4;
    *(uint32_t*)&wBuff[index] = (uint32_t)(meter.md[i].watt * 100);      //[0.00, 42,949,672.95]
    index += 4;
    *(uint32_t*)&wBuff[index] = (uint32_t)(meter.md[i].wattHour * 100);  //[0.00, 42,949,672.95]
    index += 4;
    *(int16_t*)&wBuff[index] = (int16_t)(meter.md[i].pf * 100);         //[-1.28,1.27]
    index += 2;
    *(uint32_t*)&wBuff[index] = (uint32_t)(meter.md[i].varh * 100);      //[0.00, 42,949,672.95]
    index += 4;
    *(uint32_t*)&wBuff[index] = (uint16_t)(meter.md[i].i0 * 100);        //[0.00, 42,949,672.95]
    index += 4;
    *(uint32_t*)&wBuff[index] = (uint16_t)(meter.md[i].i1 * 100);        //[0.00, 42,949,672.95]
    index += 4;
    *(uint32_t*)&wBuff[index] = (uint16_t)(meter.md[i].i2 * 100);        //[0.00, 42,949,672.95]
    index += 4;
    *(uint16_t*)&wBuff[index] = (uint16_t)(meter.md[i].v0 * 10);        //[0.00, 6553.5]
    index += 2;
    *(uint16_t*)&wBuff[index] = (uint16_t)(meter.md[i].v1 * 10);        //[0.00, 6553.5]
    index += 2;
    *(uint16_t*)&wBuff[index] = (uint16_t)(meter.md[i].v2 * 10);        //[0.00, 6553.5]
    index += 2;
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
  }
  else 
  {
    fErrorCnt++;
    Serial.println("Flash Write Not OK!!!!");
    UNDERLINE;
    if(fErrorCnt >= FLASH_ERROR_MAX)
    {
      fErrorCnt = 0;
      wIndex += wIndex += sd.cfgG.numMeter;
      setWIndex();
    }
  }
  
  return true;
}

void flashToPost(uint32_t num)
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

    playload = playload + "\"w\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"wh\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"pf\":" + String(((float)(*(int16_t*)&rBuff[index]))/100, 2) + ",";
    index += 2;

    playload = playload + "\"varh\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"i0\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"i1\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"i2\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"v0\":" + String(((float)(*(uint16_t*)&rBuff[index]))/10, 1) + ",";
    index += 2;

    playload = playload + "\"v1\":" + String(((float)(*(uint16_t*)&rBuff[index]))/10, 1) + ",";
    index += 2;

    playload = playload + "\"v2\":" + String(((float)(*(uint16_t*)&rBuff[index]))/10, 1) + "}]";
    index += 2;

    for(int i = index; (i%RECSIZE)>0; i++)
    {
      index++;
    }

    Serial.print("HOST: ");
    Serial.println(host);

    Serial.print("PATH: ");
    Serial.println(path);

    Serial.print("PL: ");
    Serial.println(playload);

    HTTPClient http;

    http.begin("http://" + host + path);
    http.addHeader("Content-Type", "application/json");

    uint16_t httpCode = http.POST(playload);
    if (httpCode != 200) {
      Serial.println("ENRES Error code: " + String(httpCode) + " ros : " + http.getString());
      return;
    } else
    {
      Serial.println("ENRES POST ok: " + String(httpCode) + " ros : " + http.getString());
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

void flashToBatchPost(uint32_t num)
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

    playload = playload + "\"w\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"wh\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"pf\":" + String(((float)(*(int16_t*)&rBuff[index]))/100, 2) + ",";
    index += 2;

    playload = playload + "\"varh\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"i0\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"i1\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"i2\":" + String(((float)(*(uint32_t*)&rBuff[index]))/100, 2) + ",";
    index += 4;

    playload = playload + "\"v0\":" + String(((float)(*(uint16_t*)&rBuff[index]))/10, 1) + ",";
    index += 2;

    playload = playload + "\"v1\":" + String(((float)(*(uint16_t*)&rBuff[index]))/10, 1) + ",";
    index += 2;

    playload = playload + "\"v2\":" + String(((float)(*(uint16_t*)&rBuff[index]))/10, 1) + "}";
    index += 2;

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

  Serial.print("PL: ");
  Serial.println(playload);

  HTTPClient http;

  http.begin("http://" + host + path);
  http.addHeader("Content-Type", "application/json");

  uint16_t httpCode = http.POST(playload);
  if (httpCode != 200) {
    Serial.println("ENRES Error code: " + String(httpCode) + " ros : " + http.getString());
    return;
  } else
  {
    Serial.println("ENRES POST ok: " + String(httpCode) + " ros : " + http.getString());
  }
  http.end();
  UNDERLINE;
  
  rIndex += num;
  // Save to RTC
  setRIndex();
  Serial.printf("Next Read Index : %d\r\n", rIndex);
  UNDERLINE;
}

// If use HW RTC must update this function
void wifiConnect()
{
  uint8_t wifiCnt = 10; // 5 Seccond
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
    UNDERLINE;
  }
}

void ReadMeterToFlash_Task()
{
  /////////////// Energy Meter /////////////////
  for(uint8_t i = 0; i<sd.cfgG.numMeter; i++)
  {
    if(!meter.readMeterData(i, sd.cfgM[i].id, sd.cfgM[i].type, mktime(&timeinfo), sd.cfgM[i].adjust))
    {
      Serial.printf("Read Meter ID:%d OK\r\n", sd.cfgM[i].id);
    } 
    else
    {
      Serial.printf("Read Meter ID:%d Error\r\n", sd.cfgM[i].id);
      return;
    }
    esp_task_wdt_feed();
    delay(50); // Must add delay when switch slave ID
  }
  UNDERLINE;

  // Write data to flash << Must Check Data already read and update
  if(dataToFlash()) 
    lastTime = mktime(&timeinfo);
}

void ReadFlashToPost_Task()
{
  static uint32_t disCnt = 0;
  
  if(wifiMulti.run() == WL_CONNECTED)
  {
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
        flashToPost(numPost);
      }
      else
      {
        if(numPost > sd.cfgG.batch) numPost = sd.cfgG.batch;
        Serial.printf("Number(s) to POST : %d\r\n", numPost);
        flashToBatchPost(numPost);
      }
    }
    disCnt = millis();
  }
  else
  {
    // Retry Every 5 Sec.
    if(millis()-disCnt > WIFIRETRY)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("WiFi Not Connecting");
      UNDERLINE;
      disCnt = millis();
    }
  }
}

void setup()
{
  delay(1000);
  pinMode(MAX485_DE_RE, OUTPUT);
  // Init in receive mode
  digitalWrite(MAX485_DE_RE, LOW);

  // Init WiFi LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Init debug port
  Serial.begin(115200, SERIAL_8N1);
  // Init Modbus communication runs at 9600 baud

  // Init Modbus Meter Comunication
  //Serial2.begin(9600, SERIAL_8E1);
  //pinMode(16, INPUT_PULLUP);

  const uart_port_t uart_num = UART_NUM_2;
  uart_config_t uart_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_EVEN,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
  };
  uart_param_config(uart_num, &uart_config);
  uart_set_pin(uart_num, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0);
  
  uart_disable_rx_intr(UART_NUM_0);
  //uart_enable_rx_intr(UART_NUM_0);

  meter.begin(Serial2);
  meter.preTransmission(preTransmission);
  meter.postTransmission(postTransmission);

  // Init SD Configuration
  sd.begin(SD_CS, Serial);
  if(!sd.readConfig("/config"))
  {
    delay(1000);
    ESP.restart();
  }
  UNDERLINE;

  // Init Flash
  if(!flash.begin(RECSIZE, Serial)) 
  {
    delay(1000);
    ESP.restart();
  }
  UNDERLINE;

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

  wifiConnect();

  now = rtc.now();
  configTime((-1)*GMT*3600, 0, "pool.ntp.org");
  // loop while NTP not sync or rtc not set
  Serial.println("Wait for time sync");
  Serial.printf("Year : %d\r\n", now.year());
  while(!(getLocalTime(&timeinfo, 10000) || (now.year() >= 2017)))
  {
    wifiMulti.run();
  }

  esp_task_wdt_feed();
}

void loop()
{ 
  if(getLocalTime(&timeinfo, 0))
  {
    // Set to HWRTC check mon and year
    if(tAdj == false)
    {
      Serial.printf("Adjust Time : %d/%d/%d %d:%d:%d\r\n", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      rtc.adjust(DateTime(timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      tAdj = true;
      lastTime = 0; // Read Meter after sync rtc
      lastUpdateRTC = mktime(&timeinfo);
    }

    // Check Update RTC Every Hour
    if(mktime(&timeinfo)-lastUpdateRTC >= (60*60))
    {
      Serial.printf("Adjust Time : %d/%d/%d %d:%d:%d\r\n", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      rtc.adjust(DateTime(timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      lastUpdateRTC = mktime(&timeinfo);
    }
  }
  else
  {
    now = rtc.now();

    timeinfo.tm_year  = now.year()-1900;
    timeinfo.tm_mon   = now.month()-1; 
    timeinfo.tm_mday  = now.day(); 
    timeinfo.tm_hour  = now.hour();
    timeinfo.tm_min   = now.minute();
    timeinfo.tm_sec   = now.second();
  }

  esp_task_wdt_feed();

  if(mktime(&timeinfo)-lastTime >= sd.cfgG.interval)
  {
    Serial.println(&timeinfo);
    UNDERLINE;
    ReadMeterToFlash_Task();
  }

  esp_task_wdt_feed();

  if(mktime(&timeinfo)-lastTime < sd.cfgG.interval-POSTSAVETIME)
  {
    ReadFlashToPost_Task();
  }
}
