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
const int analogReadPin1 = 32;               // read for set options Soil Moisture or else ...
const int analogReadPin2 = 33;               // read for set options Soil Moisture or else ...
const int analogReadPin3 = 34;               // read for set options Soil Moisture or else ...
const int RELAY1 = 15;                       // 5 = D1
const int RELAY2 = 16;                       // 
const int RELAY3 = 17;                       // 

// soil moisture variables
int minADC = 0;                       // replace with min ADC value read in air
int maxADC = 3440;                     // replace with max ADC value read fully submerged in water
int soilMoistureSetPoint = 50;
int soilMoisture, mappedValue1, mappedValue2, mappedValue3;
int range = 20;

const long interval = 1000;
int ledState = LOW;
unsigned long previousMillis = 0;

int offline = 0;
BlynkTimer timerStatus, timerCheckConnection;
WidgetLED led1(20);
WidgetLED led2(21);
WidgetLED led3(22);

char hostString[16] = {0};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
  
  meta.version = 1;

  configManager.setAPName("ogosense");
  configManager.setAPFilename("/index.html");
  configManager.addParameter("auth", config.auth, 64);
  // configManager.addParameter("ssid", config.ssid, 32);
  // configManager.addParameter("password", config.password, 64);
  configManager.addParameter("version", &meta.version, get);
  
  configManager.setAPCallback(createCustomRoute);
  configManager.setAPICallback(createCustomRoute);

  configManager.begin(config);

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
    timerCheckConnection.setInterval(15000L, checkBlynkConnection);
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

  timerStatus.setInterval(1000L, soilMoistureSensor);
  soilMoistureSensor();
}

void loop() {
  // put your main code here, to run repeatedly:

  configManager.loop();
  blink();

  if (Blynk.connected()) {
    Blynk.run();
  }
  timerCheckConnection.run();
  timerStatus.run();
}

void soilMoistureSensor()
{
  soilMoistureSensor1();
  soilMoistureSensor2();
  soilMoistureSensor3();
}

void soilMoistureSensor1()
{
  soilMoisture = analogRead(analogReadPin1);
  Serial.print("Analog Read 1 : ");
  Serial.print(soilMoisture);
  Serial.print(", " );

  mappedValue1 = map(soilMoisture, minADC, maxADC, 0, 100);


  // print mapped results to the serial monitor:
  Serial.print("Moisture value 1 = " );
  Serial.println(mappedValue1);

  if (mappedValue1 > (soilMoistureSetPoint + range)) {
    Serial.println("High Moisture");
    Serial.println("Soil Moisture: Turn Relay Off");
    digitalWrite(RELAY1, LOW);
    led1.off();
  }
  else if (mappedValue1 < (soilMoistureSetPoint - range)) {
    Serial.println("Low Moisture");
    Serial.println("Soil Moisture: Turn Relay On");
    digitalWrite(RELAY1, HIGH);
    led1.on();
  }
}

void soilMoistureSensor2()
{
  soilMoisture = analogRead(analogReadPin2);
  Serial.print("Analog Read 2 : ");
  Serial.print(soilMoisture);
  Serial.print(", " );

  mappedValue2 = map(soilMoisture, minADC, maxADC, 0, 100);


  // print mapped results to the serial monitor:
  Serial.print("Moisture value 2 = " );
  Serial.println(mappedValue2);

  if (mappedValue2 > (soilMoistureSetPoint + range)) {
    Serial.println("High Moisture");
    Serial.println("Soil Moisture: Turn Relay Off");
    digitalWrite(RELAY2, LOW);
    led2.off();
  }
  else if (mappedValue2 < (soilMoistureSetPoint - range)) {
    Serial.println("Low Moisture");
    Serial.println("Soil Moisture: Turn Relay On");
    digitalWrite(RELAY2, HIGH);
    led2.on();
  }
}

void soilMoistureSensor3()
{
  soilMoisture = analogRead(analogReadPin3);
  Serial.print("Analog Read 3 : ");
  Serial.print(soilMoisture);
  Serial.print(", " );

  mappedValue3 = map(soilMoisture, minADC, maxADC, 0, 100);


  // print mapped results to the serial monitor:
  Serial.print("Moisture value 3 = " );
  Serial.println(mappedValue3);

  if (mappedValue3 > (soilMoistureSetPoint + range)) {
    Serial.println("High Moisture");
    Serial.println("Soil Moisture: Turn Relay Off");
    digitalWrite(RELAY3, LOW);
    led3.off();
  }
  else if (mappedValue3 < (soilMoistureSetPoint - range)) {
    Serial.println("Low Moisture");
    Serial.println("Soil Moisture: Turn Relay On");
    digitalWrite(RELAY3, HIGH);
    led3.on();
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
  if (WiFi.status() == WL_CONNECTED) {    
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
    offline = 0;
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

  wifiConnect();

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
  Blynk.virtualWrite(V10, mappedValue1);
}

BLYNK_READ(V11)
{
  Blynk.virtualWrite(V11, mappedValue2);
}

BLYNK_READ(V12)
{
  Blynk.virtualWrite(V12, mappedValue3);
}

const char mimeHTML[] PROGMEM = "text/html";
const char mimeJSON[] PROGMEM = "application/json";
const char mimePlain[] PROGMEM = "text/plain";
const char configHTML[] = "<!DOCTYPE html>\n<html lang=\"en\">\n    <head>\n        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n        <style>\n            body {\n                color: #434343;\n                font-family: \"Helvetica Neue\",Helvetica,Arial,sans-serif;\n                font-size: 14px;\n                line-height: 1.42857142857143;\n                padding: 20px;\n            }\n\n            .container {\n                margin: 0 auto;\n                max-width: 400px;\n            }\n\n            form .field-group {\n                box-sizing: border-box;\n                clear: both;\n                padding: 4px 0;\n                position: relative;\n                margin: 1px 0;\n                width: 100%;\n            }\n\n            form .field-group > label {\n                color: #757575;\n                display: block;\n                margin: 0 0 5px 0;\n                padding: 5px 0 0;\n                position: relative;\n                word-wrap: break-word;\n            }\n\n            input[type=text] {\n                background: #fff;\n                border: 1px solid #d0d0d0;\n                border-radius: 2px;\n                box-sizing: border-box;\n                color: #434343;\n                font-family: inherit;\n                font-size: inherit;\n                height: 2.14285714em;\n                line-height: 1.4285714285714;\n                padding: 4px 5px;\n                margin: 0;\n                width: 100%;\n            }\n\n            input[type=text]:focus {\n                border-color: #4C669F;\n                outline: 0;\n            }\n\n            .button-container {\n                box-sizing: border-box;\n                clear: both;\n                margin: 1px 0 0;\n                padding: 4px 0;\n                position: relative;\n                width: 100%;\n            }\n\n            button[type=submit] {\n                box-sizing: border-box;\n                background: #f5f5f5;\n                border: 1px solid #bdbdbd;\n                border-radius: 2px;\n                color: #434343;\n                cursor: pointer;\n                display: inline-block;\n                font-family: inherit;\n                font-size: 14px;\n                font-variant: normal;\n                font-weight: 400;\n                height: 2.14285714em;\n                line-height: 1.42857143;\n                margin: 0;\n                padding: 4px 10px;\n                text-decoration: none;\n                vertical-align: baseline;\n                white-space: nowrap;\n            }\n        </style>\n        <title>ConfigManager</title>\n        <script type=\"text/javascript\">\n          function submitform() {\n              var formData = JSON.stringify($(\"#configForm\").serializeArray());\n\n              \n              $.ajax({\n                type: \"POST\",\n                url: \"/settings\",\n                data: formData,\n                success: function(){},\n                dataType: \"json\",\n                contentType : \"application/json\"\n              });\n\n              /*\n              fetch(\"/settings\", {\n                method: \"POST\",\n                body: JSON.stringify(formData),\n                headers: {\n                  \"Content-Type\": \"application/json\"\n                }\n              });\n              */\n          }\n\n        </script>\n    </head>\n    <body>\n        <div class=\"container\">\n            <h1 style=\"text-align: center;\">Blynk Auth Details</h1>\n            <form method=\"post\" action=\"/settings\" name=\"configForm\">\n                <div class=\"field-group\">\n                    <label>Blynk Auth</label>\n                    <input name=\"auth\" type=\"text\" size=\"32\">\n                </div>\n\n                <div class=\"button-container\">\n                    <button type=\"submit\" onclick=\"submitform()\">Save</button>\n                </div>\n            </form>\n        </div>\n    </body>\n</html>\n";



void createCustomRoute(WebServer *server) {
    Serial.println("Config API Callback");
    
    server->on("/config", HTTPMethod::HTTP_GET, [server](){
        // server->send(200, "text/plain", "Hello, World!\n");
        server->send(200, "text/html", configHTML);

        /*
        SPIFFS.begin();
        File f = SPIFFS.open("/config.html", "r");
        if (!f) {
            Serial.println(F("file open failed"));
            server->send(404, FPSTR(mimeHTML), F("File not found"));
            return;
        }
    
        server->streamFile(f, FPSTR(mimeHTML));
        f.close();
        */
    });

    server->on("/save", HTTPMethod::HTTP_POST, [server](){
        bool isJson = server->header("Content-Type") == FPSTR(mimeJSON);
        String blynkAuth;
        char blynkAuthChar[33];
        if (isJson) {
          JsonObject& obj = this->decodeJson(server->arg("plain"));
          blynkAuth = obj.get<String>("auth");
        }
        else {
          blynkAuth = server->arg("auth");
        }
        if (blynkAuth.length() == 0) {
          server->send(400, FPSTR(mimePlain), F("Invalid ssid or password."));
          return;
        }
        strncpy(blynkAuthChar, blynkAuth.c_str(), sizeof(blynkAuthChar));
        // try to save config

                
        server->send(204, FPSTR(mimePlain), F("Saved. Will attempt to reboot."));
        ESP.restart();
    });
}
