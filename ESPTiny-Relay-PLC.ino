#define _VERSION "1.0.0 (03/03/2026)"

#define DEBUG 0
#define ADDONS 1
#define TIMECLIENT_NTP 1
#define EMAILCLIENT_SMTP 0
#define EEPROM_ID 0xAC01  //Identify Sketch by EEPROM
#define WPA2ENTERPRISE 0
#define UART_BAUDRATE 115200
//#define ARDUINO_SIGNING 0

#include <WiFi.h>
#if WPA2ENTERPRISE
#include "esp_eap_client.h"  //WPA2 Enterprise
#endif
#include <LittleFS.h>
#include <EEPROM.h>
#include <AsyncTCP.h>
#include <Update.h>
#include <StreamString.h>

#include <Wire.h>
#include <DS1307.h>
DS1307 rtc;

#define SPIFFS LittleFS
#define LFS_VERSION 0x0002000b
#define U_FS U_SPIFFS

#if TIMECLIENT_NTP
#include <time.h>
#endif

#if ADDONS
typedef void (*addon_func_t)(void);
#endif

#if EMAILCLIENT_SMTP
#include <ESP_Mail_Client.h>
//Must be defined globally, otherwise "Fatal exception 28(LoadProhibitedCause)"
SMTPSession smtp;
ESP_Mail_Session session;
SMTP_Message message;
#endif

#include <ESPAsyncWebServer.h>  //Latest from (mathieucarbou/ESPAsyncWebServer)
static AsyncWebServer server(80);

#if CONFIG_IDF_TARGET_ESP32S2
#define ledPin 15
char GPIO_ARRAY[32] = "1,2,4,6,7,10,13,14";
static uint8_t RelayPin[] = { 15, 15, 15, 15, 15, 15, 15, 15 };
#else  //WROOM32
#define ledPin 23
char GPIO_ARRAY[32] = "16,16,16,16,16,16,16,16";
static uint8_t RelayPin[] = { 23, 23, 23, 23, 23, 23, 23, 23 };
#endif

#include <Ticker.h>
Ticker thread[8];
void runRelay(const uint8_t pin, const uint8_t state, uint32_t duration, const uint8_t transistor);
void runRelay(const uint8_t pin, const uint8_t state, uint32_t duration, const uint8_t transistor, const uint8_t threaded);
Ticker bthread;
void blinkThread(const uint16_t duration);
static char logbuffer[64];

const char text_html[] PROGMEM = "text/html";
const char text_plain[] PROGMEM = "text/plain";
const char text_json[] PROGMEM = "application/json";
const char locked_html[] PROGMEM = "Locked";
const char refresh_http[] PROGMEM = "Refresh";
/*
----------------
Integer Maximums
----------------
uint8 = 0 - 127
uint16 = 0 - 32,767
uint32 = 0 - 2,147,483,647
*/
//ESP32, RTC memory is only retained across deep sleep.
RTC_DATA_ATTR struct {
  time_t runTime;      //lastEpoch
  uint16_t alertTime;  //prevent email spam
} rtcData;
unsigned long webTimer = 0;                        //track last webpage access
unsigned long delayBetweenWiFi = 8 * 60 * 1000UL;  // 8 minutes

#define MAX_RULES 16

typedef struct {
  uint8_t relay;
  char month[32];
  char monthday[32];
  char weekday[32];
  char action[8];
  char type[8];
  char start_hour[3];
  char start_minute[3];
  char end_hour[3];
  char end_minute[3];
  time_t start_epoch;
  time_t end_epoch;
} plc_rule_t;

plc_rule_t rules[MAX_RULES];
int rule_count = 0;

#define _EEPROM_ID 0
#define _WIRELESS_MODE 1
#define _WIRELESS_HIDE 2
#define _WIRELESS_PHY_MODE 3
#define _WIRELESS_PHY_POWER 4
#define _WIRELESS_CHANNEL 5
#define _WIRELESS_SSID 6
#define _WIRELESS_USERNAME 7
#define _WIRELESS_PASSWORD 8
#define _LOG_ENABLE 9
#define _NETWORK_DHCP 10
#define _NETWORK_IP 11
#define _NETWORK_SUBNET 12
#define _NETWORK_GATEWAY 13
#define _NETWORK_DNS 14
//#define _RESERVED 15
//#define _RESERVED 16
//#define _RESERVED 17
#define _GPIO_ARRAY 18
#define _DEEP_SLEEP 19
#define _EMAIL_ALERT 20
#define _SMTP_SERVER 21
#define _SMTP_USERNAME 22
#define _SMTP_PASSWORD 23
#define _RELAY_NAME 24
#define _ALERTS 25
#define _DEMO_PASSWORD 26
#define _TIMEZONE_OFFSET 27

const int NVRAM_Map[] = {
  0,    //_EEPROM_ID 16
  16,   //_WIRELESS_MODE 8
  24,   //_WIRELESS_HIDE 8
  32,   //_WIRELESS_PHY_MODE 8
  40,   //_WIRELESS_PHY_POWER 8
  48,   //_WIRELESS_CHANNEL 8
  56,   //_WIRELESS_SSID 32
  88,   //_WIRELESS_USERNAME 96
  184,  //_WIRELESS_PASSWORD 48
  232,  //_LOG_ENABLE 8
  240,  //_NETWORK_DHCP 8
  304,  //_NETWORK_IP 64
  368,  //_NETWORK_SUBNET 64
  432,  //_NETWORK_GATEWAY 64
  496,  //_NETWORK_DNS 64
  512,  //_RESERVED 16
  528,  //_RESERVED 16
  560,  //_RESERVED 16
  576,  //_GPIO_ARRAY 48
  624,  //_DEEP_SLEEP 32
  656,  //_EMAIL_ALERT 64
  720,  //_SMTP_SERVER 64
  784,  //_SMTP_USERNAME 96
  880,  //_SMTP_PASSWORD 32
  912,  //_RELAY_NAME 32
  928,  //_ALERTS 16
  960,  //_DEMO_PASSWORD 32
  992,  //_TIMEZONE_OFFSET 32
  1024  //+1
};

uint8_t WIRELESS_MODE = 0;  //WIRELESS_AP = 0, WIRELESS_STA(WPA2) = 1, WIRELESS_STA(WPA2 ENT) = 2, WIRELESS_STA(WEP) = 3
//uint8_t WIRELESS_HIDE = 0;
uint8_t WIRELESS_PHY_MODE = 2;    //WIRELESS_PHY_MODE_11B = 1, WIRELESS_PHY_MODE_11G = 2, WIRELESS_PHY_MODE_11N = 3
uint8_t WIRELESS_PHY_POWER = 10;  //Max = 20.5dBm (some ESP modules 24.0dBm) should be multiples of 0.25
uint8_t WIRELESS_CHANNEL = 7;
char WIRELESS_SSID[16] = "Relay";
char WIRELESS_USERNAME[] = "";
char WIRELESS_PASSWORD[] = "";
uint8_t LOG_ENABLE = 0;  //data logger (enable/disable)
//uint8_t NETWORK_DHCP = 0;
char NETWORK_IP[64] = "192.168.8.8";  //IPv4
char NETWORK_SUBNET[64] = "255.255.255.0";
//char NETWORK_GATEWAY[] = "";
//char NETWORK_DNS[] = "";
uint32_t DEEP_SLEEP = 1;  //auto sleep timer - seconds (saved in minutes)
//=============================
//String EMAIL_ALERT = "";
//String SMTP_SERVER = "";
//String SMTP_USERNAME = "";
//String SMTP_PASSWORD = "";
char RELAY_NAME[32] = "";
char ALERTS[] = "000000000";  //dhcp-ip, low-power, -, relay-run, -, -, internal-errors, high-priority, -
//=============================
char DEMO_PASSWORD[32] = "";  //public demo
//=============================
uint16_t delayBetweenAlertEmails = 3600;  //1 hour

#if (CONFIG_IDF_TARGET_ESP32S2 && ARDUINO_ESP32_MAJOR >= 3)
#include "driver/temperature_sensor.h"
temperature_sensor_handle_t temp_handle = NULL;
temperature_sensor_config_t temp_sensor = {
  .range_min = 0,
  .range_max = 50,
};
#endif

void setup() {

#if DEBUG
  Serial.begin(UART_BAUDRATE, SERIAL_8N1);
  Serial.setDebugOutput(true);
#endif

  //======================
  //NVRAM type of Settings
  //======================
  EEPROM.begin(1024);
  long eid = atoi(NVRAMRead(_EEPROM_ID));
#if DEBUG
  Serial.print("EEPROM CRC Stored: 0x");
  Serial.println(eid, HEX);
  Serial.print("EEPROM CRC Calculated: 0x");
  Serial.println(EEPROM_ID, HEX);
#endif
  if (eid != EEPROM_ID) {
    //Check for multiple Relay SSIDs
    //WiFi.mode(WIFI_STA);
    //WiFi.disconnect();

    uint8_t n = WiFi.scanNetworks();
    for (uint8_t i = 0; i < n; ++i) {
#if DEBUG
      Serial.println(WiFi.SSID(i));
#endif
      if (WiFi.SSID(i) == WIRELESS_SSID) {
        snprintf(WIRELESS_SSID, sizeof(WIRELESS_SSID), "%s-%u", WIRELESS_SSID, i);
        break;
      }
    }
    //WiFi.scanDelete();
    LittleFS.begin(true);
    //LittleFS.format();

    NVRAM_Erase();
    NVRAMWrite(_EEPROM_ID, EEPROM_ID);
    NVRAMWrite(_WIRELESS_MODE, WIRELESS_MODE);
    NVRAMWrite(_WIRELESS_HIDE, "0");
    NVRAMWrite(_WIRELESS_PHY_MODE, WIRELESS_PHY_MODE);
    NVRAMWrite(_WIRELESS_PHY_POWER, WIRELESS_PHY_POWER);
    NVRAMWrite(_WIRELESS_CHANNEL, WIRELESS_CHANNEL);
    NVRAMWrite(_WIRELESS_SSID, WIRELESS_SSID);
    NVRAMWrite(_WIRELESS_USERNAME, "");
    NVRAMWrite(_WIRELESS_PASSWORD, "");
    NVRAMWrite(_LOG_ENABLE, "0");
    NVRAMWrite(_GPIO_ARRAY, GPIO_ARRAY);
    //==========
    NVRAMWrite(_NETWORK_DHCP, "0");
    NVRAMWrite(_NETWORK_IP, NETWORK_IP);
    NVRAMWrite(_NETWORK_SUBNET, NETWORK_SUBNET);
    NVRAMWrite(_NETWORK_GATEWAY, NETWORK_IP);
    NVRAMWrite(_NETWORK_DNS, NETWORK_IP);
    //==========
    NVRAMWrite(_DEEP_SLEEP, DEEP_SLEEP);
    //==========
    NVRAMWrite(_ALERTS, ALERTS);
    NVRAMWrite(_EMAIL_ALERT, "");
    NVRAMWrite(_SMTP_SERVER, "");
    NVRAMWrite(_SMTP_USERNAME, "");
    NVRAMWrite(_SMTP_PASSWORD, "");
    NVRAMWrite(_RELAY_NAME, WIRELESS_SSID);
    //==========
    NVRAMWrite(_DEMO_PASSWORD, DEMO_PASSWORD);
    NVRAMWrite(_TIMEZONE_OFFSET, "UTC7");
    //==========
    memset(&rtcData, 0, sizeof(rtcData));  //reset RTC memory
  } else {
    loadRelayGPIO();
    DEEP_SLEEP = atoi(NVRAMRead(_DEEP_SLEEP)) * 60;
    LOG_ENABLE = atoi(NVRAMRead(_LOG_ENABLE));
    strncpy(ALERTS, NVRAMRead(_ALERTS), sizeof(ALERTS));
    strncpy(RELAY_NAME, NVRAMRead(_RELAY_NAME), sizeof(RELAY_NAME));
  }
  //EEPROM.end();

  time_t epoch = rtcData.runTime;  //DS1307 not found
  Wire.begin();
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() == 0) {
    rtc.begin();
    rtc.getTime();
    struct tm timeinfo = {
      .tm_sec = rtc.second,  //0-59
      .tm_min = rtc.minute,  //0-59
      .tm_hour = rtc.hour,   //0-23
      .tm_mday = rtc.dayOfMonth,
      .tm_mon = rtc.month - 1,
      .tm_year = rtc.year + 100,
      .tm_wday = rtc.dayOfWeek - 1
    };
    epoch = mktime(&timeinfo);  //Convert to epoch
  }
  setSystemTime(epoch);
  setupPLC();

  esp_reset_reason_t wakeupReason = esp_reset_reason();  // ESP.getResetReason();
#if (CONFIG_IDF_TARGET_ESP32S2 && ARDUINO_ESP32_MAJOR >= 3)
  temperature_sensor_install(&temp_sensor, &temp_handle);
#endif
  /*
    REANSON_DEFAULT_RST = 0, // normal startup by power on
    REANSON_WDT_RST = 1, // hardware watch dog reset
    REANSON_EXCEPTION_RST = 2, // exception reset, GPIO status won't change
    REANSON_SOFT_WDT_RST = 3, // software watch dog reset, GPIO status won't change
    REANSON_SOFT_RESTART = 4, // software restart ,system_restart , GPIO status won't change
    REANSON_DEEP_SLEEP_AWAKE = 5, // wake up from deep-sleep
    REANSON_HARDWARE_RST = 6, // wake up by RST to GND
  */
#if DEBUG
  Serial.printf("Wakeup Reason:%u\n", wakeupReason);
#endif
  if (wakeupReason == ESP_RST_DEEPSLEEP) {  //ESP_RST_DEEPSLEEP (8)
    delayBetweenWiFi = 0;
  } else {
    for (int i = 0; i < sizeof(RelayPin); i++) {
      pinMode(RelayPin[i], INPUT_PULLUP);  //Float the pin until set NPN or PNP
    }
#if ADDONS
    // Crash detected - clear addons
    if (wakeupReason == ESP_RST_PANIC || wakeupReason == ESP_RST_TASK_WDT || wakeupReason == ESP_RST_INT_WDT || wakeupReason == ESP_RST_BROWNOUT) {
      searchAddons(true);
    }
    searchAddons(false);
#endif
    //Emergency Recover (RST to GND)
    if (wakeupReason == ESP_RST_EXT) {       //ESP_RST_EXT (2) ESP_RST_SW (3)
      ALERTS[0] = '1';                       //email DHCP IP
      ALERTS[1] = '0';                       //low voltage
      memset(&rtcData, 0, sizeof(rtcData));  //reset RTC memory (set all zero)
      setupWiFi(22);
      blinky(1200, 1);
      //ArduinoOTA.begin();
    } else {
      setupWiFi(0);
    }
    setupWebServer();
  }
#if DEBUG
  Serial.printf("Boot calibration (milliseconds):%u\n", millis());
#endif
}

void setSystemTime(time_t epoch) {
  struct timeval tv;
  const char *tz = NVRAMRead(_TIMEZONE_OFFSET);
  //char tz[] = "PST8PDT,M3.2.0,M11.1.0";

  setenv("TZ", tz, 1);
  tzset();

  tv.tv_sec = epoch;        // full seconds
  tv.tv_usec = 0;           // no microseconds available
  settimeofday(&tv, NULL);  // set ESP32 system time
}

//This is a power expensive function 80+mA
void setupWiFi(uint8_t timeout) {

  blinky(200, 3);  //Alive blink

  WIRELESS_MODE = atoi(NVRAMRead(_WIRELESS_MODE));
  WIRELESS_CHANNEL = atoi(NVRAMRead(_WIRELESS_CHANNEL));
  WIRELESS_PHY_MODE = atoi(NVRAMRead(_WIRELESS_PHY_MODE));
  WIRELESS_PHY_POWER = atoi(NVRAMRead(_WIRELESS_PHY_POWER));
  strncpy(NETWORK_IP, NVRAMRead(_NETWORK_IP), sizeof(NETWORK_IP));

  //Forcefull Wakeup
  //-------------------
  //WiFi.persistent(false);
  //WiFi.setSleepMode(WIRELESS_NONE_SLEEP);
  //WiFi.forceSleepWake();
  //-------------------
  IPAddress ip, gateway, subnet, dns;
  ip.fromString(NETWORK_IP);
  subnet.fromString(NVRAMRead(_NETWORK_SUBNET));
  gateway.fromString(NVRAMRead(_NETWORK_GATEWAY));
  dns.fromString(NVRAMRead(_NETWORK_DNS));
  //-------------------
  WiFi.persistent(false);  //Do not write settings to memory
  //0    (for lowest RF power output, supply current ~ 70mA
  //20.5 (for highest RF power output, supply current ~ 80mA
  WiFi.setTxPower((wifi_power_t)WIRELESS_PHY_POWER);

  strncpy(WIRELESS_SSID, NVRAMRead(_WIRELESS_SSID), sizeof(WIRELESS_SSID));
  strncpy(WIRELESS_PASSWORD, NVRAMRead(_WIRELESS_PASSWORD), sizeof(WIRELESS_PASSWORD));

  if (WIRELESS_MODE == 0) {
    //=====================
    //WiFi Access Point Mode
    //=====================
    uint8_t WIRELESS_HIDE = atoi(NVRAMRead(_WIRELESS_HIDE));

    //WiFi.enableSTA(false);
    //WiFi.enableAP(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, gateway, subnet);
    bool ok = WiFi.softAP(WIRELESS_SSID, WIRELESS_PASSWORD, WIRELESS_CHANNEL, WIRELESS_HIDE, 3);  //max 3 clients

#if DEBUG
    Serial.println(ok ? "AP OK" : "AP FAILED");
    Serial.println(WIRELESS_SSID);
    Serial.println(WiFi.softAPIP());
    Serial.println(WiFi.macAddress());
#endif
    //delay(100);  //Wait 100 ms for AP_START
  } else {
    //================
    //WiFi Client Mode
    //================

    //WiFi.enableSTA(true);
    //WiFi.enableAP(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();

    uint8_t NETWORK_DHCP = atoi(NVRAMRead(_NETWORK_DHCP));
    if (NETWORK_DHCP == 0) {
      WiFi.config(ip, gateway, subnet, dns);
    }

#if WPA2ENTERPRISE
    if (WIRELESS_MODE == 2) {  // WPA2-Enterprise
      const char *WIRELESS_USERNAME = NVRAMRead(_WIRELESS_USERNAME);
      WiFi.begin(WIRELESS_SSID, WPA2_AUTH_PEAP, WIRELESS_USERNAME, WIRELESS_USERNAME, WIRELESS_PASSWORD, "");

      // WPA2 Enterprise with PEAP
      //WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD, root_ca, client_cert, client_key);

      // TLS with cert-files and no password
      //WiFi.begin(ssid, WPA2_AUTH_TLS, EAP_IDENTITY, NULL, NULL, root_ca, client_cert, client_key);

    } else {
      WiFi.begin(WIRELESS_SSID, WIRELESS_PASSWORD);
    }
#else
    WiFi.begin(WIRELESS_SSID, WIRELESS_PASSWORD);
#endif

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
#if DEBUG
      /*
      0 = WL_IDLE_STATUS
      1 = WL_NO_SSID_AVAIL
      6 = WL_WRONG_PASSWORD
      */
      Serial.println(WiFi.status());
      //WiFi.printDiag();
#endif
      delay(500);

      if (timeout > 21) {
#if DEBUG
        Serial.println("Connection Failed! Rebooting...");
#endif
        //If client mode fails ESP32 will not be accessible
        //Set Emergency AP SSID for re-configuration
        NVRAMWrite(_WIRELESS_MODE, "0");
        NVRAMWrite(_WIRELESS_HIDE, "0");
        NVRAMWrite(_WIRELESS_SSID, RELAY_NAME);
        NVRAMWrite(_WIRELESS_PASSWORD, "");
        NVRAMWrite(_NETWORK_DHCP, "0");
        //delay(100);
        ESP.restart();
      }
      WIRELESS_PHY_POWER++;  //auto tune wifi power (minimum power to reach AP)
      setupWiFi(timeout++);
      return;
    }
    NVRAMWrite(_WIRELESS_PHY_POWER, WIRELESS_PHY_POWER);  //save auto tuned wifi power

    WiFi.setAutoReconnect(true);

    //NTP Client to get time
#if TIMECLIENT_NTP
    //Set time via NTP, as required for x.509 validation
    int tzOffset = -7;
    const char *tz = NVRAMRead(_TIMEZONE_OFFSET);
    if (strncmp(tz, "UTC", 3) == 0) {
      tzOffset = atoi(tz + 3);  // read number after UTC
      tzOffset = -tzOffset;     // invert sign
    }
    configTime(tzOffset * 3600, 0, "pool.ntp.org");  // offset in seconds

    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
      delay(500);
    }
#if DEBUG
    time_t now;
    time(&now);
    Serial.printf("Current time: %s", ctime(&now));
#endif
#endif
    WiFi.localIP().toString().toCharArray(NETWORK_IP, sizeof(NETWORK_IP));
#if EMAILCLIENT_SMTP
    if (ALERTS[0] == '1')
      smtpSend("DHCP IP", NETWORK_IP, 1);
#endif
#if DEBUG
    Serial.println(WiFi.localIP().toString());
#endif
  }
}

void setupWebServer() {
  //LittleFSConfig config;
  //LittleFS.setConfig(config);
  LittleFS.begin(true);

  strncpy(DEMO_PASSWORD, NVRAMRead(_DEMO_PASSWORD), sizeof(DEMO_PASSWORD));
  //==============================================
  //Async Web Server HTTP_GET, HTTP_POST, HTTP_ANY
  //==============================================
  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream(FPSTR(text_plain));
    webTimer = millis();
    if (request->hasParam("temp")) {
      float tempC = 0;
#if (CONFIG_IDF_TARGET_ESP32S2 && ARDUINO_ESP32_MAJOR >= 3)
      temperature_sensor_enable(temp_handle);
      temperature_sensor_get_celsius(temp_handle, &tempC);
      temperature_sensor_disable(temp_handle);
#endif
      response->printf("%.2f", tempC);
    } else if (request->hasParam("clock")) {
      /*
      byte error, address;
      int nDevices;
      response->println("Scanning...");
      nDevices = 0;
      for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
          response->print("I2C device found at address 0x");
          if (address < 16) response->print("0");
          response->print(address, HEX);
          response->println("  !");
          nDevices++;
        } else if (error == 4) {
          response->print("Unknown error at address 0x");
          if (address < 16) response->print("0");
          response->println(address, HEX);
        }
      }
      if (nDevices == 0) {
        response->println("No I2C devices found\n");
      }else{
      */
      Wire.beginTransmission(0x68);
      uint8_t active = Wire.endTransmission();
      if (active == 0) {
        rtc.getTime();
        response->printf("UTC Date: %02d-%02d-%04d Time: %02d:%02d:%02d DOW: %d", rtc.month, rtc.dayOfMonth, (rtc.year + 100 + 1900), rtc.hour, rtc.minute, rtc.second, (rtc.dayOfWeek - 1));
      } else {
        response->printf("I2C Error: %u", active);
      }
      //}
    } else if (request->hasParam("ntp")) {
      if (request->hasParam("tz")) {
        const char *tz = request->getParam("tz")->value().c_str();
        NVRAMWrite(_TIMEZONE_OFFSET, tz);
      }
      if (request->hasParam("epoch")) {
        time_t epoch = atoi(request->getParam("epoch")->value().c_str());
        setSystemTime(epoch);
      }
      time_t now;
      time(&now);                          // get current system time (UTC internally)
      struct tm *timeinfo = gmtime(&now);  // interprets as UTC
      if (request->hasParam("epoch")) {
        rtc.fillByYMD((timeinfo->tm_year + 1900), (timeinfo->tm_mon + 1), timeinfo->tm_mday);
        rtc.fillByHMS(timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        rtc.fillDayOfWeek(timeinfo->tm_wday + 1);
        rtc.setTime();
        //rtc.startClock();
      }
      timeinfo = localtime(&now);  // converts UTC to local time using TZ
      response->printf("Date: %02d-%02d-%04d Time: %02d:%02d:%02d DOW: %d", (timeinfo->tm_mon + 1), timeinfo->tm_mday, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, timeinfo->tm_wday);
    } else if (strlen(DEMO_PASSWORD) == 0) {
      if (request->hasParam("reset")) {
        NVRAM_Erase();
        //NVRAMWrite(_PNP, PNP);
        bthread.detach();
        bthread.attach(2, []() {
          ESP.restart();
        });
        response->print(F("..."));
      } else if (request->hasParam("smtp")) {
#if EMAILCLIENT_SMTP
        thread.once(1, []() {
          smtpSend("Test", "OK", 1);
        });
        response->print(F("..."));
#else
          response->print(F("No SMTP"));
#endif
      } else if (request->hasParam("addons")) {
        String filepath = "/addons/";
        if (request->hasParam("run")) {
          filepath += request->getParam("run")->value();
          File file = LittleFS.open(filepath);
          if (file) {
            runAddon(file);
          }
          response->print(F("OK"));
        } else if (request->hasParam("remove")) {
          filepath += request->getParam("remove")->value();
          if (LittleFS.remove(filepath)) {
            response->print(F("OK"));
          }
        } else {
          File root = LittleFS.open("/addons");
          File file = root.openNextFile();
          while (file) {
            response->printf("%s\n", file.name());
            file = root.openNextFile();
          }
        }
      } else if (request->hasParam("gpio")) {
        if (request->hasParam("save")) {
          const char *gpioParam = request->getParam("save")->value().c_str();
          NVRAMWrite(_GPIO_ARRAY, gpioParam);
          loadRelayGPIO();
        } else {
          uint8_t count = 8;  //sizeof(RelayPin) / sizeof(RelayPin[0]);
          for (uint8_t i = 0; i < count; i++) {
            response->printf("%d", RelayPin[i]);
            if (i < count - 1) {
              response->printf(",");
            }
          }
        }
      } else if (request->hasParam("plc")) {
        setupPLC();
        applyPLC();
        for (uint8_t i = 0; i < rule_count; i++) {
          plc_rule_t *r = &rules[i];
          //response->printf("[%u] %d - %d\n",  r->relay, r->start_epoch, r->end_epoch);
          response->printf("[#%u] %02d:%02d - %02d:%02d %s -> %s\n", r->relay, atoi(r->start_hour), atoi(r->start_minute), atoi(r->end_hour), atoi(r->end_minute), r->action, timePLC(r) ? "TRUE" : "FALSE");
        }
      } else if (request->hasParam("relay")) {
        const AsyncWebParameter *testRelay = request->getParam(0);
        uint8_t n = atoi(request->getParam("n")->value().c_str());
        //TODO: Ask user with pop-up
        //uint8_t transistor = atoi(request->getParam("pnp")->value().c_str());
#if CONFIG_IDF_TARGET_ESP32S2
        uint8_t transistor = 1;
#else  //WROOM32
          uint8_t transistor = 0;
#endif
        //0 = stop, 1 = run
        if (testRelay->value() == "0") {
          thread[n].detach();
          runRelayFinish(RelayPin[n], 1, transistor);
        } else if (testRelay->value() == "1") {
          runRelay(RelayPin[n], 1, UINT8_MAX, transistor, n);
        }
        response->print(F("..."));
      }
    } else {
      response->print(FPSTR(locked_html));
    }
    request->send(response);
  });
  server.on("/plc.txt", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream(FPSTR(text_plain));

    if (strlen(DEMO_PASSWORD) == 0) {
      String plcText;
      if (request->hasParam("plcbox", true)) {  // true = POST body
        plcText = request->getParam("plcbox", true)->value();
      }
      File file = LittleFS.open("/plc.txt", "w");
      if (file) {
        file.print(plcText);
        file.close();
      }
      //setupPLC();
      //applyPLC();
      response->addHeader(FPSTR(refresh_http), "4;url=/");
      response->print("Saved ...");
    } else {
      response->print(FPSTR(locked_html));
    }
    request->send(response);
  });
  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("end")) {
      LOG_ENABLE = 0;
      LittleFS.remove("/l");
      //NVRAMWrite(_LOG_ENABLE, "0");
    } else if (request->hasParam("start")) {
      LOG_ENABLE = 1;
      dataLog("l");
      //NVRAMWrite(_LOG_ENABLE, "1");
    } else if (LittleFS.exists("/l")) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/l", FPSTR(text_plain));
      request->send(response);
      return;
    }
    request->send(200, FPSTR(text_plain), F("..."));
  });
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->getParam("password", true)->value() == DEMO_PASSWORD) {
      DEMO_PASSWORD[0] = 0;  //reset
    }
    request->redirect("/");
  });
  server.on("/nvram.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->params() > 0) {
      if (strlen(DEMO_PASSWORD) == 0) {
        uint8_t i = atoi(request->getParam("offset")->value().c_str());
        if (request->hasParam("alert")) {
          ALERTS[i] = atoi(request->getParam("alert")->value().c_str());
          NVRAMWrite(_ALERTS, ALERTS);
        } else {
          /*
          const char *s = request->getParam("value")->value().c_str();
          char *endptr;
          int32_t v = strtol(s, &endptr, 10);  // base 10
          if (*endptr == '\0') {
            NVRAMWrite(i, v);
          } else {
            NVRAMWrite(i, s);
          }
          */
          NVRAMWrite(i, request->getParam("value")->value().c_str());
        }
        request->send(200, FPSTR(text_plain), request->getParam("value")->value());
      } else {
        request->send(200, FPSTR(text_plain), FPSTR(locked_html));
      }
    } else {
      AsyncResponseStream *response = request->beginResponseStream(FPSTR(text_json));
      response->print(F("{\"nvram\": [\""));
      //esp_chip_info_t chip_info;
      //esp_chip_info(&chip_info);
      response->printf("%d.%d.%d %s", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH, ESP.getChipModel());
      //uint16_t major = (LFS_VERSION >> 16) & 0xFFFF; // 0x0002
      //uint16_t minor = LFS_VERSION & 0xFFFF;         // 0x0005
      //response->printf("%u.%u", major, minor);
      response->printf("|%s|%u|%s|%u|%u\"", ESP.getSdkVersion(), LFS_VERSION, _VERSION, ESP.getFreeSketchSpace(), ESP.getFreeHeap());  //esp_himem_get_free_size()
#if DEBUG
      Serial.printf("Flash free: %6d bytes\r\n", ESP.getFreeSketchSpace());
      Serial.printf("DRAM free: %6d bytes\r\n", ESP.getFreeHeap());
#endif
      for (uint8_t i = 1; i <= 27; i++) {
        if (i == _WIRELESS_PASSWORD || i == _SMTP_PASSWORD || i == _DEMO_PASSWORD) {
          if (strlen(DEMO_PASSWORD) == 0) {
            response->print(F(",\"\""));
          } else {
            response->print(F(",\"****\""));
          }
        } else {
          response->printf(",\"%s\"", NVRAMRead(i));
        }
      }
      response->print(F("]}"));
      request->send(response);
    }
  });
  /*
  server.on("/nvram", HTTP_GET, [](AsyncWebServerRequest *request) {
    char out[2048];
    size_t len = 0;
    for (uint32_t i = 0; i < EEPROM.length(); i++) {
      len += snprintf(out + len, sizeof(out) - len, "%02X", EEPROM.read(i));
    }
    request->send(200, FPSTR(text_plain), out);
  });
  */
  server.on("/nvram", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream(FPSTR(text_plain));
    if (strlen(DEMO_PASSWORD) > 0) {
      response->print(FPSTR(locked_html));
    } else {
      const AsyncWebParameter *param0 = request->getParam(0);
      if (!param0) return;

      char buf[64];
      size_t len = 0;
      uint8_t n = 1, skip = 28;

      len = snprintf(buf, sizeof(buf), "6;url=/");
      if (param0->name() == "wifi") {
        const AsyncWebParameter *modeParam = request->getParam("Mode", true);
        const AsyncWebParameter *dhcpParam = request->getParam("DHCP", true);
        if (modeParam->value() == "0") {
          bthread.detach();
          bthread.attach(4, []() {
            ESP.restart();
          });
        } else if (dhcpParam->value() == "1") {
          len += snprintf(buf + len, sizeof(buf) - len, "find.html");
        } else {
          const AsyncWebParameter *ipParam = request->getParam("WiFiIP", true);
          if (ipParam) {
            len--;  // remove last character
            len += snprintf(buf + len, sizeof(buf) - len, "http://%s", ipParam->value());
          }
        }
      } else if (param0->name() == "alert") {
        n = _EMAIL_ALERT;
        skip = (_EMAIL_ALERT - _SMTP_PASSWORD) + 1;  //skip oauth token
      } else if (param0->name() == "demo") {
        n = _DEMO_PASSWORD;
      }
      response->addHeader(FPSTR(refresh_http), buf);

      for (size_t i = 1; i < request->params(); i++) {
        if (i != skip) {
          /*
          const char *s = request->getParam(i)->value().c_str();
          char *endptr;
          int32_t v = strtol(s, &endptr, 10);  // base 10
          if (*endptr == '\0') {
            NVRAMWrite(n, v);
            response->printf("[%d] %s:%u\n", n, s, NVRAMReadInt(n));
          } else {
            NVRAMWrite(n, s);
            response->printf("[%d] %s:%s\n", n, s, NVRAMRead(n));
          }
          */
          NVRAMWrite(n, request->getParam(i)->value().c_str());
          response->printf("[%d] %s:%s\n", n, request->getParam(i)->name().c_str(), NVRAMRead(n));
          n++;
        }
      }
#if DEBUG
      Serial.println("NVRAM Forcig Restart");
#endif
    }
    request->send(response);
  });
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    webTimer = millis();
#ifndef ARDUINO_SIGNING
    if (strlen(DEMO_PASSWORD) > 0)
      if (!request->authenticate("", DEMO_PASSWORD))
        return request->requestAuthentication();
#endif
    AsyncResponseStream *response = request->beginResponseStream(FPSTR(text_html));
    response->print(F("<!DOCTYPE html><html><body>"));
    //if (request->hasParam("boot")) {
    //  response->print("<script>fetch('reboot').then((async()=>{for(;;){if((await fetch('update')).ok){location='/'}await new Promise(r=>setTimeout(r,999))}})())</script>...");
    //} else {
    const char *labels[] = { "Firmware", "Filesystem" };
    for (uint8_t i = 0; i < 2; i++) {
      response->print(F("<form action=http://"));
      response->print(NETWORK_IP);
      response->print(request->url());
      response->print(F(" method=post enctype='multipart/form-data'><input type=file accept='.bin,.signed' name="));
      response->print(labels[i]);
      response->print(F("><input type=submit value='Update "));
      response->print(labels[i]);
      response->print(F("'></form><br>"));
      //}
    }
    response->print(F("</body></html>"));
    request->send(response);
  });
  server.on(
    "/update", HTTP_POST, [](AsyncWebServerRequest *request) {
#ifndef ARDUINO_SIGNING
      if (strlen(DEMO_PASSWORD) > 0)
        if (!request->authenticate("", DEMO_PASSWORD))
          return request->requestAuthentication();
#endif
      AsyncResponseStream *response = request->beginResponseStream(FPSTR(text_html));

      if (Update.hasError()) {
        StreamString str;
        Update.printError(str);
        response->print(str);
      } else {
        response->print(F("Update Success! ..."));
      }
      response->addHeader(FPSTR(refresh_http), "8;url=/");
      bthread.detach();
      bthread.attach(2, []() {
        ESP.restart();
      });
      request->send(response);
    },
    WebUpload);
  server.on("/", [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/index.html")) {
      request->redirect("/index.html");
    } else {
      AsyncWebServerResponse *response = request->beginResponse(200, FPSTR(text_html), F("File System Not Found ..."));
      response->addHeader(FPSTR(refresh_http), "4;url=/update");
      request->send(response);
    }
  });
  server.onNotFound([](AsyncWebServerRequest *request) {
#if DEBUG
    //Serial.println((request->method() == HTTP_GET) ? "GET" : "POST");
    Serial.printf("\nRequest: %s\n", request->url().c_str());
#endif
    String file = request->url();
    if (LittleFS.exists(file)) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, file, getContentType(file));
      if (file == "/find.html") {
        response->addHeader(F("Access-Control-Allow-Origin"), "*");  // Allow all origins
      } else if (file != "/plc.txt") {
        response->addHeader(F("Content-Encoding"), "gzip");
      }
      request->send(response);
    } else {
      request->send(404, FPSTR(text_plain), F("404: Not Found"));
    }
  });

  server.begin();  // Web server start
}

void loop() {
  //ArduinoOTA.handle();
  if ((millis() - webTimer) > delayBetweenWiFi) {  //track web activity for 5 minutes
    applyPLC();
#if ADDONS
    searchAddons(false);
#endif
    readySleep();
  }
  //delay(1);
  delay(10000);  // wait 10 seconds
}

void readySleep() {

#if EMAILCLIENT_SMTP
  if (rtcData.alertTime > delayBetweenAlertEmails) {
    rtcData.alertTime = 0;  //prevent spam and server block (but send all in same cycle)
  } else {
    rtcData.alertTime++;  //count down alert timing
  }
#endif

  bool anyThreadActive = bthread.active();
  for (uint8_t i = 0; i < 8; i++) {
    if (thread[i].active()) {
      anyThreadActive = true;
      break;
    }
  }
  if (!anyThreadActive) {
    esp_sleep_disable_wifi_wakeup();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);  //RTC memory preserved
    uint64_t sleep_us = (DEEP_SLEEP * 1000000ULL);
    esp_sleep_enable_timer_wakeup(sleep_us);
    /*
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    */
    time_t now;
    time(&now);
    rtcData.runTime = now + DEEP_SLEEP;  //add sleep time, when we wake up will be accurate.
    esp_deep_sleep_start();              //GPIO16 (D0) needs to be tied to RST to wake from deepSleep

    //TODO: Check state and use WAKE_RF_DEFAULT for second stage
    //ESP.deepSleep(DEEP_SLEEP, WAKE_RF_DEFAULT);
  } else {
    WiFi.disconnect(true);  //disassociate properly (easier to reconnect)
    WiFi.mode(WIFI_OFF);
  }
}

void dataLog(const char *text) {
  if (LOG_ENABLE == 1) {
    LittleFS.begin();
    size_t flashsize = LittleFS.totalBytes() - LittleFS.usedBytes() - strlen(text);
    if (flashsize > 1) {
      File file = LittleFS.open("/l", "a");
      if (file) {
        file.println(text);
        file.close();
      }
    } else {
      LittleFS.remove("/l");
    }
  }
}

void runRelayFinish(const uint8_t pin, const uint8_t state, const uint8_t transistor) {
  turnNPNorPNP(pin, !state, transistor);  // OFF or ON (Reverse State)
#if EMAILCLIENT_SMTP
  if (ALERTS[3] == '1')
    smtpSend("Run Pump", pin, 0);
#endif
  //snprintf(logbuffer, sizeof(logbuffer), "T:%lu,M:%u", PLANT_MANUAL_TIMER, PLANT_SOIL_MOISTURE);
  //dataLog(logbuffer);
}
void runRelay(const uint8_t pin, const uint8_t state, uint32_t duration, const uint8_t transistor) {
  while (duration--) {
    turnNPNorPNP(pin, state, transistor);  // ON or OFF
    esp_sleep_enable_timer_wakeup(1000000ULL);
    esp_light_sleep_start();  //CPU pauses
  }
  runRelayFinish(pin, state, transistor);
}

void runRelay(const uint8_t pin, const uint8_t state, uint32_t duration, const uint8_t transistor, const uint8_t threaded) {
  thread[threaded].detach();
  uint16_t counter = duration;

  thread[threaded].attach(1, [pin, state, counter, transistor, threaded]() mutable {
    // if finished, detach Ticker and reset
    if (counter == 0) {
      thread[threaded].detach();
      runRelayFinish(pin, state, transistor);
    } else {
      turnNPNorPNP(pin, state, transistor);  // ON or OFF
      counter--;
    }
  });
}

void turnNPNorPNP(const uint8_t pin, const uint8_t state, const uint8_t transistor) {
#if DEBUG
  Serial.printf("[%u]", state);
#endif
  if (transistor == 1) {
    digitalWrite(pin, !state);
    if (state == 0) {
      pinMode(pin, INPUT_PULLUP);  // Float the pin for PNP off (true high-impedance)
    } else {
      pinMode(pin, OUTPUT);
    }
  } else {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state);
  }
}

void blinky(uint16_t timer, uint16_t duration) {
  bthread.detach();
  uint16_t counter = duration * 2;  //toggle style

  pinMode(ledPin, OUTPUT);

  //mutable allows the lambda to modify its own copy of the captured variables:
  bthread.attach_ms(timer, [counter]() mutable {
#if DEBUG
    Serial.printf("blink: %u\n", counter);
#endif
    if (counter == 0) {
      bthread.detach();
      digitalWrite(ledPin, LOW);
    } else {
      digitalWrite(ledPin, (counter & 1) ? HIGH : LOW);
      counter--;
    }
  });
}
//=============
// NVRAM CONFIG
//=============
void NVRAM_Erase() {
  for (uint32_t i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0xFF);
  }
  EEPROM.commit();
}

void NVRAMWrite(uint8_t address, uint32_t value) {
  /*
  for (int i = 0; i < 4; i++) {
    EEPROM.write(NVRAM_Map[address] + i, (value >> (8 * i)) & 0xFF);  // LSB first
  }
  EEPROM.commit();
  */
  char txt[12];
  snprintf(txt, sizeof(txt), "%u", value);
  NVRAMWrite(address, txt);
}

void NVRAMWrite(uint8_t address, const char *txt) {
  /*
  int EEPROM_SIZE = 32;
  char buffer[EEPROM_SIZE];
  memset(buffer, 0, EEPROM_SIZE);
  txt.toCharArray(buffer, EEPROM_SIZE);
  EEPROM.put(address * EEPROM_SIZE, buffer);
  free(buffer);
  */
  //const int EEPROM_SIZE = 32;
  const int EEPROM_SIZE = (NVRAM_Map[(address + 1)] - NVRAM_Map[address]);
#if DEBUG
  Serial.printf("NVRAMWrite: %u > %u:%u %s\n", address, NVRAM_Map[address], EEPROM_SIZE, txt);
#endif
  int len = strlen(txt);
  for (int i = 0; i < EEPROM_SIZE; i++) {
    if (i < len) {
      EEPROM.write(NVRAM_Map[address] + i, txt[i]);
      //EEPROM.write(address * EEPROM_SIZE + i, txt[i]);
    } else {
      EEPROM.write(NVRAM_Map[address] + i, 0xFF);
      //EEPROM.write(address * EEPROM_SIZE + i, 0xFF);
      break;
    }
  }
  EEPROM.commit();
}
/*
uint32_t NVRAMReadInt(uint8_t address) {
  uint32_t readValue = 0;
  for (int i = 0; i < 4; i++) {
    readValue |= ((uint32_t)EEPROM.read(NVRAM_Map[address] + i) << (8 * i));
  }
#if DEBUG
  uint8_t EEPROM_SIZE = (NVRAM_Map[(address + 1)] - NVRAM_Map[address]);
  Serial.printf("\nNVRAMReadInt: %u > %u:%u ", address, NVRAM_Map[address], EEPROM_SIZE);
  char buf[12];
  snprintf(buf, sizeof(buf), "%u", readValue);
  for (int i = 0; i < 12; i++) {
    Serial.printf("%02X", buf[i]);  // 2-digit uppercase hex with leading zero
  }
  Serial.print(" > ");
  Serial.print(buf);
  Serial.print("\n");
#endif
  return readValue;
}
*/
char *NVRAMRead(uint8_t address) {
  /*
  int EEPROM_SIZE = 32;
  char buffer[EEPROM_SIZE];
  EEPROM.get(address * EEPROM_SIZE, buffer);
  */
  //const int EEPROM_SIZE = 32;
  uint8_t EEPROM_SIZE = (NVRAM_Map[(address + 1)] - NVRAM_Map[address]);
  static char buffer[96];

  int i = 0;
  for (i = 0; i < EEPROM_SIZE; i++) {
    uint8_t byte = EEPROM.read(NVRAM_Map[address] + i);
    //char byte = EEPROM.read(address * EEPROM_SIZE + i);
    if (byte == 0xFF) break;  // stop at empty byte
    buffer[i] = byte;
  }
  buffer[i] = '\0';
#if DEBUG
  Serial.printf("\nNVRAMRead: %u > %u:%u ", address, NVRAM_Map[address], EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) {
    Serial.printf("%02X", buffer[i]);  // 2-digit uppercase hex with leading zero
  }
  Serial.print(" > ");
  Serial.print(buffer);
  Serial.print("\n");
#endif

  return buffer;
}

String getContentType(String filename) {
  if (filename.endsWith("ml"))
    return FPSTR(text_html);
  else if (filename.endsWith("ss"))
    return F("text/css");
  else if (filename.endsWith("js"))
    return F("application/javascript");
  /*
  else if (filename.endsWith("ng"))
    return "image/png";
  else if (filename.endsWith("pg"))
    return "image/jpeg";
  */
  else if (filename.endsWith("co"))
    return F("image/x-icon");
  else if (filename.endsWith("vg"))
    return F("image/svg+xml");
  return FPSTR(text_plain);
}

//===============
//Web OTA Updater
//===============
void WebUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    //WARNING: Do not save RTC memory after Update.begin(). "chksum" will be different after reboot, firmware will not flash
    /*
    memset(&rtcData, 0, sizeof(rtcData));  //reset RTC memory (set all zero)
    ESP.rtcUserMemoryWrite(32, (uint32_t *)&rtcData, sizeof(rtcData));
    */
    if (filename.indexOf("fs") != -1) {
      //if (request->hasParam("filesystem", true)) {
      //https://github.com/ayushsharma82/ElegantOTA/blob/master/src/ElegantOTA.cpp
      size_t fsSize = UPDATE_SIZE_UNKNOWN;
#if DEBUG
      Serial.printf("Free Filesystem Space: %u\n", fsSize);
      Serial.printf("Filesystem Offset: %u\n", U_FS);
#endif
      if (!Update.begin(fsSize, U_FS))  //start with max available size
        Update.printError(Serial);
    } else {
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;  //calculate sketch space required for the update
#if DEBUG
      Serial.printf("Free Scketch Space: %u\n", maxSketchSpace);
      Serial.printf("Flash Offset: %u\n", U_FLASH);
#endif
      if (!Update.begin(maxSketchSpace, U_FLASH))  //start with max available size
        Update.printError(Serial);
    }
  }
  if (!Update.hasError())
    if (Update.write(data, len) != len)
      Update.printError(Serial);

  if (final) {
    if (!Update.end(true)) {
      Update.printError(Serial);
    }
  }
}

#if EMAILCLIENT_SMTP
void smtpSend(const char *subject, const char *body, uint8_t now) {

#if DEBUG
  Serial.printf("Email: %s\n", subject);
#endif

  if (now == 0 && rtcData.alertTime < delayBetweenAlertEmails) {
#if DEBUG
    Serial.printf("Email Timeout: %u (%u)\n", delayBetweenAlertEmails, rtcData.alertTime);
#endif
    return;
  }
  /*
  WIRELESS_MODE = NVRAMRead(_WIRELESS_MODE).toInt();
  if (WIRELESS_MODE == 0)  //cannot send email in AP mode
    return;
  */
  byte off = 0;
  if (WiFi.getMode() == WIFI_OFF)  //alerts during off cycle
  {
    setupWiFi(0);  //turn on temporary
    off = 1;
  }

#if DEBUG
  smtp.debug(1);
  Serial.printf("Unix time: %u\n", timeClient.getEpochTime());
#endif
#if TIMECLIENT_NTP
  time_t now;
  time(&now);
  smtp.setSystemTime(now);
//#else
//  session.time.ntp_server = F("pool.ntp.org");
//  session.time.gmt_offset = -8;
//  session.time.day_light_offset = 0;
#endif

  const char *smtpURL = NVRAMRead(_SMTP_SERVER);
  char smtpServer[64] = { 0 };
  uint16_t smtpPort = 25;
  const char *colon = strchr(smtpURL, ':');  // Find the colon
  if (colon) {
    size_t hostLen = colon - smtpURL;
    memcpy(smtpServer, smtpURL, hostLen);

    const char *portStr = colon + 1;  //points one character after the colon
    smtpPort = atoi(portStr);         // assumes only digits
  } else {
    // No colon found, copy full string as hostname
    strncpy(smtpServer, smtpURL, sizeof(smtpServer));
  }
  session.server.host_name = smtpServer;
  session.server.port = smtpPort;
  session.login.email = NVRAMRead(_SMTP_USERNAME);
  session.login.password = NVRAMRead(_SMTP_PASSWORD);
  /*
  File oauth = LittleFS.open("/oauth", "r");
  if (oauth) {
    session.login.accessToken = oauth.readString();  //XOAUTH2
  } else {
    session.login.password = NVRAMRead(_SMTP_PASSWORD);
  }
  */
  //session.login.user_domain = F("127.0.0.1");
  /*
  if(smtpPort > 25) {
    session.secure.mode = esp_mail_secure_mode_ssl_tls;
  }else{
    session.secure.mode = esp_mail_secure_mode_nonsecure;
  }
  */
  //File mlog = LittleFS.open("/l", "w");

  if (smtp.connect(&session)) {
    message.sender.name = RELAY_NAME;
    message.sender.email = NVRAMRead(_SMTP_USERNAME);
    message.addRecipient("", NVRAMRead(_EMAIL_ALERT));
    //message.addCc("");
    //message.addBcc("");
    message.subject = subject;
    message.text.content = body;
    if (ALERTS[7] == '1') {
      message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;
    } else {
      message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
    }
#if DEBUG
    if (!MailClient.sendMail(&smtp, &message, true)) {
      Serial.println(smtp.errorReason());
    }
#else
    MailClient.sendMail(&smtp, &message, true);
    //mlog.print(smtp.errorReason());
#endif
    smtp.sendingResult.clear();  //clear sending result log
    //}else{
    //  mlog.print(String(smtp.statusCode()));
    //  mlog.print(String(smtp.errorCode()));
    //  mlog.print(smtp.errorReason());
  }
  //mlog.close();

  if (off == 1)
    WiFi.mode(WIFI_OFF);
}
#endif

// Trim leading and trailing spaces (handles multiple spaces)
char *trim(char *s) {
  while (*s == ' ' || *s == '\t') s++;  // leading
  char *end = s + strlen(s) - 1;
  while (end > s && (*end == ' ' || *end == '\t')) *end-- = 0;  // trailing
  return s;
}

int weekdayStrToNum(const char *s) {
  if (strcmp(s, "SUN") == 0) return 0;
  if (strcmp(s, "MON") == 0) return 1;
  if (strcmp(s, "TUE") == 0) return 2;
  if (strcmp(s, "WED") == 0) return 3;
  if (strcmp(s, "THU") == 0) return 4;
  if (strcmp(s, "FRI") == 0) return 5;
  if (strcmp(s, "SAT") == 0) return 6;
  return -1;
}

int matchWeekDay(plc_rule_t *r, int wday) {
  // wday = 0-6 (Sun-Sat)
  if (strcmp(r->weekday, "*") == 0) return 1;

  char copy[32];
  strcpy(copy, r->weekday);
  char *token = strtok(copy, ",");
  while (token) {
    char start[4], end[4];
    if (sscanf(token, "%3[^-]-%3s", start, end) == 2) {
      int ws = weekdayStrToNum(start);
      int we = weekdayStrToNum(end);
      if (ws <= wday && wday <= we) return 1;
    } else {
      if (wday == weekdayStrToNum(token)) return 1;
    }
    token = strtok(NULL, ",");
  }
  return 0;
}

int matchMonth(plc_rule_t *r, int month) {
  // month = 1-12 (current day)
  if (strcmp(r->month, "*") == 0) return 1;

  char copy[32];
  strcpy(copy, r->month);
  char *token = strtok(copy, ",");
  while (token) {
    int start = 0, end = 0;
    if (sscanf(token, "%d-%d", &start, &end) == 2) {
      if (month >= start && month <= end) return 1;
    } else {
      if (atoi(token) == month) return 1;
    }
    token = strtok(NULL, ",");
  }
  return 0;
}

int matchMonthDay(plc_rule_t *r, int mday) {
  if (strcmp(r->monthday, "*") == 0) return 1;

  char copy[32];
  strcpy(copy, r->monthday);
  char *token = strtok(copy, ",");
  while (token) {
    int start = 0, end = 0;
    if (sscanf(token, "%d-%d", &start, &end) == 2) {
      if (mday >= start && mday <= end) return 1;
    } else {
      if (atoi(token) == mday) return 1;
    }
    token = strtok(NULL, ",");
  }
  return 0;
}

//-----------------------------
// Parse one line from PLC file
void parseRule(char *line) {
  if (!line || strlen(line) < 5) return;  // skip empty lines

  plc_rule_t *r = &rules[rule_count];
  char *token;

  // --- Relay ---
  token = strtok(line, ":");
  if (!token) return;
  sscanf(trim(token), "Relay%d", &r->relay);
  //char tmp[2];
  //tmp[0] = token[5];  // single character
  //tmp[1] = '\0';      // null terminator
  //r->relay = atoi(tmp);

  // --- Month ---
  token = strtok(NULL, ":");
  if (!token) return;
  strcpy(r->month, trim(token));

  // --- MonthDay ---
  token = strtok(NULL, ":");
  if (!token) return;
  strcpy(r->monthday, trim(token));

  // --- Weekday ---
  token = strtok(NULL, ":");
  if (!token) return;
  strcpy(r->weekday, trim(token));

  // --- Time ---
  token = strtok(NULL, ":");  // Time -> "17:40-01:00"
  if (!token) return;
  token = trim(token);
  r->start_hour[0] = token[0];
  r->start_hour[1] = token[1];
  r->start_hour[2] = '\0';
  r->start_minute[0] = token[3];
  r->start_minute[1] = token[4];
  r->start_minute[2] = '\0';
  r->end_hour[0] = token[6];
  r->end_hour[1] = token[7];
  r->end_hour[2] = '\0';
  r->end_minute[0] = token[9];
  r->end_minute[1] = token[10];
  r->end_minute[2] = '\0';
  token = strtok(NULL, ":");
  token = strtok(NULL, ":");

  // --- Action ---
  token = strtok(NULL, ":");
  if (!token) return;
  strcpy(r->action, trim(token));

  // --- Type ---
  token = strtok(NULL, ":");
  if (!token) return;
  strcpy(r->type, trim(token));

  // --- Rule Matches Today ---
  time_t now;
  time(&now);
  struct tm now_tm;
  localtime_r(&now, &now_tm);
  if (matchMonth(r, (now_tm.tm_mon + 1)) && matchMonthDay(r, now_tm.tm_mday) && matchWeekDay(r, now_tm.tm_wday)) {
    rule_count++;
  }
}

#if ADDONS
void runAddon(File &file) {
  String filename = file.name();
#if DEBUG
  Serial.print("Loading ");
  Serial.println(filename);
#endif
  size_t size = file.size();
  uint8_t *buffer = (uint8_t *)ps_malloc(size);  // Try PSRAM
  if (!buffer) {
    buffer = (uint8_t *)malloc(size);  // Fallback
  }
  if (!buffer) {
#if DEBUG
    Serial.println("Memory allocation failed");
#endif
    return;
  }
  file.read(buffer, size);
#if DEBUG
  Serial.print("Calling ");
  Serial.println(filename);
#endif
  /*
    addon_func_t* func_table = (addon_func_t*)buffer;
    size_t func_count = 1;
    for (size_t i = 0; i < func_count; i++) {
      func_table[i](); // call addon function
    }
    */
  addon_func_t func = (addon_func_t)buffer;
  //func();
  free(buffer);
}

void searchAddons(bool remove) {
  File root = LittleFS.open("/addons");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      if (remove) {
        file.close();
        String fn = file.name();
        LittleFS.remove("/addons/" + fn);
      } else {
        runAddon(file);
        file.close();
      }
      file = root.openNextFile();
    }
  }
}
#endif

uint32_t calculateDurationSeconds(const char *start_h, const char *start_m, const char *end_h, const char *end_m) {
  uint8_t sh = atoi(start_h);
  uint8_t sm = atoi(start_m);
  uint8_t eh = atoi(end_h);
  uint8_t em = atoi(end_m);

  uint32_t start_sec = sh * 3600 + sm * 60;
  uint32_t end_sec = eh * 3600 + em * 60;

  // Handle overnight interval
  if (end_sec <= start_sec) {
    end_sec += 24 * 3600;
  }

  return end_sec - start_sec;
}

void loadRelayGPIO() {
  strncpy(GPIO_ARRAY, NVRAMRead(_GPIO_ARRAY), sizeof(GPIO_ARRAY));

  char buf[32] = { 0 };                       // buffer to copy the string
  strncpy(buf, GPIO_ARRAY, sizeof(buf) - 1);  // ensure null-terminated
  int count = 0;
  char *token = strtok(buf, ",");
  while (token != NULL && count < 8) {
    RelayPin[count++] = atoi(token);  // convert to integer
    token = strtok(NULL, ",");
  }
}

bool timePLC(plc_rule_t *r) {
  time_t now;
  time(&now);
  struct tm *now_tm = localtime(&now);
  int now_minutes = now_tm->tm_hour * 60 + now_tm->tm_min;
  int start_minutes = atoi(r->start_hour) * 60 + atoi(r->start_minute);
  int end_minutes = atoi(r->end_hour) * 60 + atoi(r->end_minute);
  int active = false;

  // Overnight case: end < start (e.g., 17:40-01:00)
  if (end_minutes < start_minutes) {
    // Current time is either after start or before end next day
    if (now_minutes >= start_minutes || now_minutes <= end_minutes) {
      active = true;
    }
  } else {
    // Normal same-day interval
    if (now_minutes >= start_minutes && now_minutes <= end_minutes) {
      active = true;
    }
  }
  return active;
}

void applyPLC() {
  for (int i = 0; i < rule_count; i++) {
    plc_rule_t *r = &rules[i];
    uint8_t relay = r->relay - 1;  //convert relay# to array#
    uint8_t transistor = (r->type[0] == 'P') ? 1 : 0;
    if (timePLC(r)) {
      uint32_t duration = calculateDurationSeconds(r->start_hour, r->start_minute, r->end_hour, r->end_minute);
      if (r->action[0] == 'O' && r->action[1] == 'N') {
        runRelay(RelayPin[relay], 1, duration, transistor, relay);
      } else {
        runRelay(RelayPin[relay], 0, duration, transistor, relay);
      }
#if DEBUG
      Serial.print("Relay ");
      Serial.print(r->relay);
      Serial.println(" ACTIVE");
#endif
    } else {
      thread[relay].detach();
      runRelayFinish(RelayPin[relay], 1, transistor);
    }
  }
}

void setupPLC() {
  if (!LittleFS.begin(true)) {
    return;
  }

  File file = LittleFS.open("/plc.txt", "r");
  if (!file) {
    return;
  }
  rule_count = 0;

  char line[128];
  while (file.available()) {
    if (rule_count >= MAX_RULES) break;

    int len = file.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = 0;

    if (line[0] == ';' || len < 5) continue;

    parseRule(line);
  }
  file.close();
}
