/*
  MIT License
Version 1.0 2018-12-14
Copyright (c) 2018 kaebmoo gmail com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


/*
 * Hardware
 * Wemos D1 mini, Pro (esp8266)
 * soilWatch 10 use A0 pin
 * Wemos Relay Shield use D1 pin
 *
 *
 *
 */

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266WiFiMulti.h>

#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <EEPROM.h>
#include <BlynkSimpleEsp8266.h>
#include <ThingSpeak.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include <Wire.h>
#include <Adafruit_ADS1015.h>


// #define BLYNKLOCAL
#define SLEEP
#define THINGSBOARD
// #define THINGSPEAK

const int FW_VERSION = 1;  // 2018 12 1 version 1.0
const char* firmwareUrlBase = "http://www.ogonan.com/ogoupdate/";

#ifdef ARDUINO_ESP8266_WEMOS_D1MINI
  String firmware_name = "ogoSwitch_soilMoistureSensor.ino.d1_mini";
#elif ARDUINO_ESP8266_WEMOS_D1MINILITE
  String firmware_name = "ogoSwitch_soilMoistureSensor.ino.d1_minilite";
#elif ARDUINO_ESP8266_WEMOS_D1MINIPRO
  String firmware_name = "ogoSwitch_soilMoistureSensor.ino.d1_minipro";
#endif

char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 360709;
char *readAPIKey = "GNZ8WEU763Z5DUEA";
char *writeAPIKey = "8M07EYX8NPCD9V8U";

char thingsboardServer[40] = "box.greenwing.email";
int  mqttport = 1883;                      // 1883 or 1888
char token[32] = "MhqKwhgcDr5Y6uKYQ5d4";   // device token from thingsboard server

char c_thingsboardServer[41] = "192.168.1.10";
char c_mqttport[8] = "1883";
char c_token[33] = "12345678901234567890";
char c_sendinterval[8] = "10000";
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "634021991b694e08b004ca8b13f08bc1";
char c_auth[33] = "634021991b694e08b004ca8b13f08bc1";           // authen token blynk
char c_writeAPIKey[17] = "";                                    // write api key thingspeak
char c_sleepSeconds[8] = "300";

//flag for saving data
bool shouldSaveConfig = false;
int sendinterval = 60;                                       // send data interval time in second


#define TRIGGER_PIN D3                      // GPIO 0
const int analogReadPin = A0;               // read for set options Soil Moisture or else ...
#ifndef SLEEP
const int RELAY1 = D1;                      // GPIO 5
#endif

#ifdef SLEEP
const int RELAY1 = D7;
#endif


#define GPIO0 16                            // D0
#define GPIO2 2                             // D4

#define GPIO0_PIN D0
#define GPIO2_PIN D4
// We assume that all GPIOs are LOW
boolean gpioState[] = {false, false};
const int MAXRETRY=5;

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

char *mqtt_user = "seal";
char *mqtt_password = "sealwiththekiss";
const char* mqtt_server = "db.ogonan.com";


char *myRoom = "sensor/mySpUo9axZsy36zSgxY0Lm3ZI2DiOJSD";
char *roomStatus = "sensor/mySpUo9axZsy36zSgxY0Lm3ZI2DiOJSD/status";
int mqtt_reconnect = 0;

WiFiClient client;
PubSubClient mqttClient(client);
ESP8266WiFiMulti wifiMulti;
#ifdef SLEEP
  // sleep for this many seconds default is 300 (5 minutes)
  int sleepSeconds = 15;
  Adafruit_ADS1015 ads1015;   // Construct an ads1015 at the default address: 0x48
#endif


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(RELAY1, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
  digitalWrite(RELAY1, 0);
  delay(1000);
  /*
  myRoom = (char *) malloc(sizeof(strlen("sensor/") + strlen(String(ESP.getChipId()).c_str())+1 ));
  strcpy(myRoom, "sensor/");
  strcat(myRoom, String(ESP.getChipId()).c_str());
  Serial.print("sensor name: ");
  Serial.println(myRoom);
  */

  readConfig();
  wifiConnect();
  if (offline == 0) {
    #ifdef BLYNKLOCAL
    Blynk.config(auth, "blynk.ogonan.com", 80);  // in place of Blynk.begin(auth, ssid, pass);
    #endif
    #ifdef BLYNK
    Blynk.config(auth);  // in place of Blynk.begin(auth, ssid, pass);
    #endif
    #if defined(BLYNKLOCAL) || defined(BLYNK)
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
    #endif

    
    #ifdef THINGSPEAK
    ThingSpeak.begin( client );
    timerStatus.setInterval(sendinterval * 1000, sendThingSpeak);
    #endif
    #ifdef THINGSBOARD
    setup_mqtt();
    timerStatus.setInterval(sendinterval * 1000, sendSoilMoistureData);
    #endif

    #if defined(BLYNKLOCAL) || defined(BLYNK)
    checkConnectionTimer.setInterval(15000L, checkBlynkConnection);
    #endif

    #ifdef SLEEP
      ads1015.begin();  // Initialize ads1015    
      soilMoistureSensor();
      #ifdef THINGSPEAK
      // send data to thingspeak
      sendThingSpeak();
      #endif

      #ifdef THINGSBOARD
      // send data to thingsboard
      if ( !mqttClient.connected() ) {
        reconnect();
      }
      sendSoilMoistureData();
      #endif
    
    Serial.println("I'm going to sleep.");
    Serial.println("Goodnight folks!");
    
    delay(15000);
    ESP.deepSleep(sleepSeconds * 1000000);
    #endif
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  wifiMulti.run();

  #ifdef THINGSBOARD
  if ( !mqttClient.connected() ) {
    reconnect();
  }
  
  mqttClient.loop();
  #endif
  soilMoistureSensor();
  delay(1000);
  blink();

  #if defined(BLYNKLOCAL) || defined(BLYNK)
  if (Blynk.connected()) {
    Blynk.run();
  }
  #endif
  checkConnectionTimer.run();
  timerStatus.run();
}

void soilMoistureSensor()
{
  soilMoisture = analogRead(analogReadPin);
  Serial.print("Analog Read A0: ");
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

void sendSoilMoistureData()
{


  // Just debug messages
  Serial.println("Send soil moisture data.");
  Serial.print( "[ ") ;
  Serial.print( mappedValue );
  Serial.print( "]   -> " );

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"soil moisture\":";  payload += mappedValue; payload += ",";
  #ifdef SLEEP
  float volt = checkBattery();
  payload += "\"battery\":"; payload += volt; payload += ",";
  #endif
  payload += "\"active\":"; payload += digitalRead(RELAY1) ? true : false;
  payload += "}";

  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 128 );
  mqttClient.publish( "v1/devices/me/telemetry", attributes );
  Serial.println( attributes );
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
  String SSID = WiFi.SSID();
  String PSK = WiFi.psk();
  
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.psk());
  
  WiFi.begin();
  Serial.print("Connecting");
  Serial.println();

  wifiMulti.addAP("Red1", "12345678");
  wifiMulti.addAP("ogofarm", "ogofarm2018");
  wifiMulti.addAP("Red_Plus", "12345678");
  wifiMulti.addAP("Red", "12345678");

  Serial.println("Connecting Wifi...");

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print("1");
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      ondemandWiFi();
    }
    retry2Connect++;
    if (retry2Connect >= 30) {
      offline = 1;
      break;
    }
  }
  /*
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("1");
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      ondemandWiFi();
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
  */
  
  
  
  Serial.println();
  if (offline == 0) {
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  Serial.println();
}

void ondemandWiFi()
{
  WiFiManager wifiManager;

  wifiManager.setBreakAfterConfig(true);
  wifiManager.setConfigPortalTimeout(60);
  
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", c_thingsboardServer, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", c_mqttport, 7);
  WiFiManagerParameter custom_sendinterval("interval", "send data interval time", c_sendinterval, 7);
  WiFiManagerParameter custom_c_auth("c_auth", "Auth Token", c_auth, 37);
  WiFiManagerParameter custom_thingsboard_token("token", "thingsboard token", c_token, 32);
  WiFiManagerParameter custom_c_writeapikey("c_writeapikey", "Write API Key : ThingSpeak", c_writeAPIKey, 17);
  #ifdef SLEEP
  WiFiManagerParameter custom_c_sleepSeconds("c_sleepSeconds", "Sleep Interval (second)", c_sleepSeconds, 7);
  #endif

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_sendinterval);
  wifiManager.addParameter(&custom_c_auth);
  wifiManager.addParameter(&custom_thingsboard_token);
  wifiManager.addParameter(&custom_c_writeapikey);
  #ifdef SLEEP
  wifiManager.addParameter(&custom_c_sleepSeconds);
  #endif

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
    strcpy(c_thingsboardServer, custom_mqtt_server.getValue());
    strcpy(c_mqttport, custom_mqtt_port.getValue());
    strcpy(c_token, custom_thingsboard_token.getValue());
    strcpy(c_sendinterval, custom_sendinterval.getValue());
    strcpy(c_auth, custom_c_auth.getValue());
    strcpy(c_writeAPIKey, custom_c_writeapikey.getValue());
    #ifdef SLEEP
    strcpy(c_sleepSeconds, custom_c_sleepSeconds.getValue());
    #endif

    strcpy(thingsboardServer, c_thingsboardServer);
    mqttport = atoi(c_mqttport);
    strcpy(token, c_token);
    sendinterval = atoi(c_sendinterval);
    strcpy(auth, c_auth);
    strcpy(writeAPIKey, c_writeAPIKey);
    #ifdef SLEEP
    sleepSeconds = atoi(c_sleepSeconds);
    #endif

    Serial.print("thingsboard server: ");
    Serial.println(c_thingsboardServer);
    Serial.print("mqtt port: ");
    Serial.println(c_mqttport);
    Serial.print("ThingsBoard device token: ");
    Serial.println(c_token);
    Serial.print("send interval: ");
    Serial.println(c_sendinterval);
    Serial.print("Blynk auth token : ");
    Serial.println(auth);
    Serial.print("Thinspeak write API Key : ");
    Serial.println(writeAPIKey);
    #ifdef SLEEP
    Serial.println("Sleep interval : ");
    Serial.print(c_sleepSeconds);
    #endif

    writeEEPROM(thingsboardServer, 0, 40);
    writeEEPROM(c_mqttport, 40, 7);
    eeWriteInt(47, sendinterval);
    writeEEPROM(auth, 60, 32);
    writeEEPROM(token, 92, 20);
    writeEEPROM(writeAPIKey, 112, 16);
    #ifdef SLEEP
    eeWriteInt(144, sleepSeconds);
    #endif
    eeWriteInt(500, 6550);
  }
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void readConfig()
{
  EEPROM.begin(512);

  memset(thingsboardServer, 0, sizeof(thingsboardServer));
  memset(c_mqttport, 0, sizeof(c_mqttport));
  memset(auth, 0, sizeof(auth));
  memset(token, 0, sizeof(token));
  memset(writeAPIKey, 0, sizeof(writeAPIKey));
  #ifdef SLEEP
  memset(c_sleepSeconds, 0, sizeof(c_sleepSeconds));
  #endif
  
  readEEPROM(thingsboardServer, 0, 40);
  readEEPROM(c_mqttport, 40, 7);
  sendinterval = eeGetInt(47);
  readEEPROM(auth, 60, 32);         // blynk auth token
  readEEPROM(token, 92, 20);        // thingsboard device token
  readEEPROM(writeAPIKey, 112, 32); // thingspeak write api key
  mqttport = atoi(c_mqttport);
  #ifdef SLEEP
  sleepSeconds = eeGetInt(144); 
  #endif

  Serial.println();
  Serial.print("thingsboard server: ");
  Serial.println(thingsboardServer);
  Serial.print("mqtt port: ");
  Serial.println(mqttport);
  Serial.print("send interval: ");
  Serial.println(sendinterval);
  Serial.print("blynk auth token: ");
  Serial.println(auth);
  Serial.print("device token: ");
  Serial.println(token);
  #ifdef SLEEP
  Serial.print("sleep interval: ");
  Serial.print(sleepSeconds);
  #endif


  int saved = eeGetInt(500);
  if (saved == 6550) {
    strcpy(c_thingsboardServer, thingsboardServer);
    itoa(mqttport, c_mqttport, 10);
    strcpy(c_auth, auth);
    strcpy(c_token, token);
    strcpy(c_writeAPIKey, writeAPIKey);
    itoa(sendinterval, c_sendinterval, 10);
    #ifdef SLEEP
    itoa(sleepSeconds, c_sleepSeconds, 10);
    #endif
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

#ifdef THINGSPEAK
void sendThingSpeak()
{
  ThingSpeak.setField( 1, mappedValue );
  ThingSpeak.setField( 2, digitalRead(RELAY1));
  #ifdef SLEEP
      float volt = checkBattery();
      ThingSpeak.setField(3, volt);
    #endif
  int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
  Serial.print("Return Code: ");
  Serial.println(writeSuccess);
  Serial.println();
}
#endif

float checkBattery()
{
  float volt = 0.0;

  #ifndef SLEEP
  unsigned int raw = 0;
  raw = analogRead(A0);
  volt = raw * (3.7 / 1023.0);
  // volt = volt * 4.2;
  Serial.print("Analog read: ");
  Serial.println(raw);
  #endif

  #ifdef SLEEP
  int16_t adc0, adc1, adc2, adc3;
  
  adc0 = ads1015.readADC_SingleEnded(0);
  adc1 = ads1015.readADC_SingleEnded(1);
  adc2 = ads1015.readADC_SingleEnded(2);
  adc3 = ads1015.readADC_SingleEnded(3);
  
  volt = ((float) adc1 * 3.0) / 1000.0;
  Serial.print("Analog read A1: ");
  Serial.println(adc1);
  #endif
  
  

  // String v = String(volt);
  Serial.print("Battery voltage: ");
  Serial.println(volt);

  return(volt);
}

void setup_mqtt()
{
  #ifdef THINGSBOARD
  Serial.println("FARM Server : "+String(thingsboardServer)+" mqtt port: "+String(mqttport));
  mqttClient.setServer(thingsboardServer, mqttport);  // default port 1883, mqtt_server, thingsboardServer
  #else
  mqttClient.setServer(mqtt_server, mqttport);
  #endif
  mqttClient.setCallback(callback);
  if (mqttClient.connect(myRoom, token, NULL)) {
    Serial.print("mqtt connected : ");
    Serial.println(thingsboardServer);  // mqtt_server
    Serial.println();
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  int i;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);

  if (methodName.equals("getGpioStatus")) {
    // Reply with GPIO status
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), get_gpio_status().c_str());
  } else if (methodName.equals("setGpioStatus")) {
    // Update GPIO status and reply
    set_gpio_status(data["params"]["pin"], data["params"]["enabled"]);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), get_gpio_status().c_str());
    mqttClient.publish("v1/devices/me/attributes", get_gpio_status().c_str());
  }

}


String get_gpio_status() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();

  data[String(GPIO0_PIN)] = gpioState[0] ? true : false;
  data[String(GPIO2_PIN)] = gpioState[1] ? true : false;

  char payload[256];
  data.printTo(payload, sizeof(payload));

  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);

  return strPayload;
}

void set_gpio_status(int pin, boolean enabled) {
  if (pin == GPIO0_PIN) {
    // Output GPIOs state
    digitalWrite(GPIO0, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[0] = enabled;
  }
  else if (pin == GPIO2_PIN) {
    // Output GPIOs state
    digitalWrite(GPIO2, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[1] = enabled;
  }

}

void reconnect()
{
  // Loop until we're reconnected


  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    #ifdef THINGSBOARD
    if (mqttClient.connect(myRoom, token, NULL)) {  // connect to thingsboards
    #else
    if (mqttClient.connect(myRoom, mqtt_user, mqtt_password)) {  // connect to thingsboards
    #endif
      Serial.print("connected : ");
      Serial.println(thingsboardServer); // mqtt_server



      // Once connected, publish an announcement...
      // mqttClient.publish(roomStatus, "hello world");

    } else {
      Serial.print("failed, reconnecting state = ");
      Serial.print(mqttClient.state());
      Serial.print(" try : ");
      Serial.print(mqtt_reconnect+1);
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
    mqtt_reconnect++;
    if (mqtt_reconnect > MAXRETRY) {
      mqtt_reconnect = 0;
      break;
    }
  }
}

void upintheAir()
{
  String fwURL = String( firmwareUrlBase );
  fwURL.concat( firmware_name );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".version" );

  Serial.println( "Checking for firmware updates." );
  // Serial.print( "MAC address: " );
  // Serial.println( mac );
  Serial.print( "Firmware version URL: " );
  Serial.println( fwVersionURL );

  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) {
    String newFWVersion = httpClient.getString();

    Serial.print( "Current firmware version: " );
    Serial.println( FW_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFWVersion );

    int newVersion = newFWVersion.toInt();

    if( newVersion > FW_VERSION ) {
      Serial.println( "Preparing to update" );

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
  }
  httpClient.end();
  // ESPhttpUpdate.update("www.ogonan.com", 80, "/ogoupdate/ogoswitch_blynk.ino.d1_mini.bin");
}

BLYNK_CONNECTED()
{
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V2);

  // Blynk.virtualWrite(V21, soilMoistureSetPoint+range);
  // Blynk.virtualWrite(V22, soilMoistureSetPoint-range);
}

BLYNK_WRITE(V1)
{
  soilMoistureSetPoint = param.asInt();
  Serial.print("Set point: ");
  Serial.println(soilMoistureSetPoint);
  Serial.println();

  Blynk.virtualWrite(V21, soilMoistureSetPoint+range);
  Blynk.virtualWrite(V22, soilMoistureSetPoint-range);
}

BLYNK_WRITE(V2)
{
  range = param.asInt();
  Serial.print("Range: ");
  Serial.println(range);
  Serial.println();

  Blynk.virtualWrite(V21, soilMoistureSetPoint+range);
  Blynk.virtualWrite(V22, soilMoistureSetPoint-range);
}


BLYNK_READ(V10)
{
  Blynk.virtualWrite(V10, mappedValue);
}
