#include <WiFi.h>
// #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
// #include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
// #include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <WiFiClient.h>
// #include <ESP8266mDNS.h>
// #include <ESP8266HTTPUpdateServer.h>
// #include <ESP8266HTTPClient.h>
// #include <ESP8266httpUpdate.h>

#include <EEPROM.h>
// #include <BlynkSimpleEsp8266.h>
#include <BlynkSimpleEsp32.h>
#include "ConfigManager.h"
#include "mdns.h"

struct Config {
    char auth[64];
    // char ssid[32];
    // char password[64];
} config;

struct Metadata {
    int8_t version;
} meta;

ConfigManager configManager;

#define BLYNKLOCAL

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "12345678901234567890abcdefghijkl";
char c_auth[33] = "12345678901234567890abcdefghijkl";           // authen token blynk
//flag for saving data
bool shouldSaveConfig = false;


#define TRIGGER_PIN 0                       // D3
const int analogReadPin = 32;               // read for set options Soil Moisture or else ...
const int RELAY1 = 5;                       // D1

// soil moisture variables
int minADC = 0;                       // replace with min ADC value read in air
int maxADC = 928;                     // replace with max ADC value read fully submerged in water
int soilMoistureSetPoint = 50;
int soilMoisture, mappedValue;
int range = 20;

const long interval = 1000;
int ledState = LOW;
unsigned long previousMillis = 0;

int offline = 0;
BlynkTimer timerStatus, checkConnectionTimer;
WidgetLED led1(20);

char hostString[16] = {0};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(RELAY1, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
  meta.version = 1;

  configManager.setAPName("ogosense");
  configManager.setAPFilename("/index.html");
  configManager.addParameter("auth", config.auth, 64);
  // configManager.addParameter("ssid", config.ssid, 32);
  // configManager.addParameter("password", config.password, 64);
  configManager.addParameter("version", &meta.version, get);
  configManager.begin(config);

  configManager.setAPCallback(createCustomRoute);
  configManager.setAPICallback(createCustomRoute);

  Serial.print("auth from config: ");
  Serial.println(config.auth);
  // Serial.print("ssid from config: ");
  // Serial.println(config.ssid);
  // Serial.print("password from config: ");
  // Serial.println(config.password);

  strcpy(auth, config.auth);
  
  digitalWrite(RELAY1, 0);
  delay(1000);
  // readConfig();
  // wifiConnect();
  if (offline == 0) {
    #ifdef BLYNKLOCAL
    Blynk.config(auth, "blynk.ogonan.com", 80);  // in place of Blynk.begin(auth, ssid, pass);
    #else
    Blynk.config(auth);  // in place of Blynk.begin(auth, ssid, pass);
    #endif
    boolean result = Blynk.connect(3333);  // timeout set to 10 seconds and then continue without Blynk, 3333 is 10 seconds because Blynk.connect is in 3ms units.
    Serial.print("Blynk connect : ");
    Serial.println(result);
    if(!Blynk.connected()){
      Serial.println("Not connected to Blynk server");
      Blynk.connect(3333);  // try to connect to server with default timeout
    }
    else {
      Serial.println("Connected to Blynk server");
    }
    checkConnectionTimer.setInterval(15000L, checkBlynkConnection);
  }

  // sprintf(hostString, "esp-%06d", ESP.getChipId());
  sprintf(hostString, "ogosense");
  Serial.print("My Hostname: ");
  Serial.println(hostString);
  WiFi.setHostname(hostString); // ESP32

  esp_err_t err = mdns_init();
  if (err) {
      printf("MDNS Init failed: %d\n", err);
  }
  //set hostname
  mdns_hostname_set("ogosense");
   
}

void loop() {
  // put your main code here, to run repeatedly:

  configManager.loop();
  soilMoistureSensor();
  delay(1000);
  blink();

  if (Blynk.connected()) {
    Blynk.run();
  }
  checkConnectionTimer.run();
  
}

void soilMoistureSensor()
{
  soilMoisture = analogRead(analogReadPin);
  Serial.print("Analog Read : ");
  Serial.print(soilMoisture);
  Serial.print(", " );

  mappedValue = map(soilMoisture, minADC, maxADC, 0, 100);


  // print mapped results to the serial monitor:
  Serial.print("Moisture value = " );
  Serial.println(mappedValue);

  if (mappedValue > (soilMoistureSetPoint + range)) {
    Serial.println("High Moisture");
    Serial.println("Soil Moisture: Turn Relay Off");
    digitalWrite(RELAY1, LOW);
    led1.off();
  }
  else if (mappedValue < (soilMoistureSetPoint - range)) {
    Serial.println("Low Moisture");
    Serial.println("Soil Moisture: Turn Relay On");
    digitalWrite(RELAY1, HIGH);
    led1.on();
  }
}

void blink()
{
  unsigned long currentMillis = millis();

  // if enough millis have elapsed
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // toggle the LED
    ledState = !ledState;
    digitalWrite(BUILTIN_LED, ledState);
  }
}

void wifiConnect()
{
  int retry2Connect = 0;
  
  Serial.println();
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.psk());
  String SSID = WiFi.SSID();
  String PSK = WiFi.psk();
  WiFi.begin();
  Serial.print("Connecting");
  Serial.println();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("1");
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      // ondemandWiFi();
    }
    retry2Connect++;
    if (retry2Connect >= 30) {
      retry2Connect = 0;
      Serial.print(" Red ");
      WiFi.begin("Red", "12345678");
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print("2");
        retry2Connect++;
        if (retry2Connect >= 30) {
          retry2Connect = 0;
          Serial.print(" ogofarm ");
          WiFi.begin("ogofarm", "ogofarm2018");
          while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print("3");
            retry2Connect++;
            if (retry2Connect >= 30) {
              offline = 1;
              break;
            }
          }
          break;
        }
      }   
      break;          
    }
  }
  Serial.println();
  if (offline == 0) {    
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
  }
}

/*
void ondemandWiFi()
{
  WiFiManager wifiManager;

  WiFiManagerParameter custom_c_auth("c_auth", "Auth Token", c_auth, 37);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_c_auth);
  delay(10);

  if (!wifiManager.startConfigPortal("ogoSwitch-SoilWatch")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  
  if (shouldSaveConfig) {
    Serial.println("Saving config...");
    strcpy(c_auth, custom_c_auth.getValue());
    strcpy(auth, c_auth);
    Serial.print("auth token : ");
    Serial.println(auth);
    writeEEPROM(auth, 60, 32);
    eeWriteInt(500, 6550);
  }
}
*/

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void readConfig()
{
  EEPROM.begin(512);
  readEEPROM(auth, 60, 32);
  Serial.print("auth token : ");
  Serial.println(auth);

  int saved = eeGetInt(500);
  if (saved == 6550) {
    strcpy(c_auth, auth);
  }
}

void readEEPROM(char* buff, int offset, int len) {
    int i;
    for (i=0;i<len;i++) {
        buff[i] = (char)EEPROM.read(offset+i);
    }
    buff[len] = '\0';
}

void writeEEPROM(char* buff, int offset, int len) {
    int i;
    for (i=0;i<len;i++) {
        EEPROM.write(offset+i,buff[i]);
    }
    EEPROM.commit();
}

void eeWriteInt(int pos, int val) {
    byte* p = (byte*) &val;
    EEPROM.write(pos, *p);
    EEPROM.write(pos + 1, *(p + 1));
    EEPROM.write(pos + 2, *(p + 2));
    EEPROM.write(pos + 3, *(p + 3));
    EEPROM.commit();
}

int eeGetInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}

int blynkreconnect = 0;
void checkBlynkConnection()
{
  int mytimeout;
  bool blynkConnectedResult = false;



  Serial.println("Check Blynk connection.");
  blynkConnectedResult = Blynk.connected();
  if (!blynkConnectedResult) {
    Serial.println("Blynk not connected");
    mytimeout = millis() / 1000;
    Serial.println("Blynk trying to reconnect.");
    while (!blynkConnectedResult) {
      blynkConnectedResult = Blynk.connect(3333);
      Serial.print(".");
      if((millis() / 1000) > mytimeout + 3) { // try for less than 4 seconds
        Serial.println("Blynk reconnect timeout.");
        break;
      }
    }
  }
  if (blynkConnectedResult) {
      Serial.println("Blynk connected");
  }
  else {
    Serial.println("Blynk not connected");
    Serial.print("blynkreconnect: ");
    Serial.println(blynkreconnect);
    blynkreconnect++;
    if (blynkreconnect >= 10) {
      blynkreconnect = 0;
      // delay(60000);
      // ESP.reset();
    }
  }
}

BLYNK_CONNECTED()
{
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V2);
}

BLYNK_WRITE(V1)
{
  soilMoistureSetPoint = param.asInt();
  Serial.print("Set point: ");
  Serial.println(soilMoistureSetPoint);
  Serial.println();
}

BLYNK_WRITE(V2)
{
  range = param.asInt();
  Serial.print("Range: ");
  Serial.println(range);
  Serial.println();
}

BLYNK_READ(V10)
{
  Blynk.virtualWrite(V10, mappedValue);
}

void createCustomRoute(WebServer *server) {
    server->on("/custom", HTTPMethod::HTTP_GET, [server](){
        server->send(200, "text/plain", "Hello, World!");
    });
}
