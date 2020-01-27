// Detect the falling edge of multiple buttons.
// Eight buttons with internal pullups.
// Toggles a LED when any button is pressed.
// Buttons on pins 2,3,4,5,6,7,8,9

// Include the Bounce2 library found here :
// https://github.com/thomasfredericks/Bounce2
#include <Bounce2.h>

/*
  MIT License
Version 1.0 2020-1-27
Copyright (c) 2020 kaebmoo gmail com

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

#define THINGSBOARD

#include <EEPROM.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <Wire.h>
#include "Adafruit_MCP23008.h"
#include <Arduino.h>
#include "Timer.h"
#include <TimeAlarms.h>
#include <ThingSpeak.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <assert.h>

WiFiClient switchClient;
Timer timer;

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 432257;
char *writeAPIKey = "";
char *readAPIKey = "";

// MicroGear microgear(switchClient);              // declare microgear object
PubSubClient mqttClient(switchClient);

char clientID[64] = "switch/curtain/trakool/";
unsigned long nodeID = 1001;
int mqttCountReconnect = 0;

char token[32] = "99999999999999999999";                    // device token from thingsboard server 
int mqttport = 1883;
char thingsboardServer[40] = "thingsboard.ogonan.com";      // "box.greenwing.email" "192.168.2.64"
int sendinterval = 60000;                                   // send data interval time ms

// mqtt
char *mqttUserName = "";
char *mqttPassword = "";
const char* mqttServer = "db.ogonan.com";

String subscribeTopic = "node/" + String( nodeID ) + "/control/messages";
String publishTopic = "node/" + String( nodeID ) + "/status/messages";


unsigned long lastMqttConnectionAttempt = 0;
unsigned long lastSend;
const int MAXRETRY=30;

// for portal config
char c_thingsboardServer[41] = "192.168.1.10";
char c_mqttport[8] = "1883";
char c_token[33] = "12345678901234567890";
char c_sendinterval[8] = "10000";                           // ms
char c_nodeid[8] = "";          // node id mqtt

int wifi_reconnect = 0;
bool shouldSaveConfig = false;



#define LED_PIN D4  // 13

#define NUM_BUTTONS 5
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {D0, D7, D5, D6, D8};

// 4 D0 limit switch 2nd floor
// 5 D7 limit switch 1st floor
// 6 D5 up
// 7 D6 down
// 3 D8 Emergency

int ledState = LOW;
// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)


Bounce * buttons = new Bounce[NUM_BUTTONS];

#define RELAY1  D1
#define RELAY2  D2
// #define LAMP1   10
// #define LAMP2   11

bool limitUp = false;
bool limitDown = false;

int limitUpPin, limitDownPin;

void setup() {

  Serial.begin(115200);
  EEPROM.begin(512);
  
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].attach( BUTTON_PINS[i] , INPUT_PULLUP  );       //setup the bounce instance for the current button
    buttons[i].interval(25);              // interval in ms
  }

  // Setup the LED :
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  // pinMode(LAMP1, OUTPUT);
  // pinMode(LAMP2, OUTPUT);
  digitalWrite(LED_PIN, ledState);
  Serial.println();
  Serial.println("Starting");
  // reset all relay
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  // digitalWrite(LAMP1, LOW);
  // digitalWrite(LAMP2, LOW);

  readConfig();
  setupWifi();
  setupMqtt();
}

void loop() {

  bool needToToggleLed = false;
  

  buttons[0].update();
  buttons[1].update();
  buttons[2].update();
  buttons[3].update();
  buttons[4].update();

  limitUpPin = digitalRead(BUTTON_PINS[0]);   // limit switch 2nd floor, Normal Open Switch - NO
  limitDownPin = digitalRead(BUTTON_PINS[1]); // limit switch 1st floor, Normal Open Switch - NO

  if (buttons[4].rose() ) {     // emergency switch
    Serial.println("Emergency Stop");
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);
    // digitalWrite(LAMP1, LOW);
    // digitalWrite(LAMP2, LOW);
  }
  if (buttons[0].rose() || limitUpPin == HIGH) { // limit switch 2nd floor, turn off
    Serial.println("Limit Switch 2nd floor");
    if (limitUp == false) {
      digitalWrite(RELAY1, LOW);
      digitalWrite(RELAY2, LOW);
      delay(500);
      // digitalWrite(LAMP1, LOW);   // Lamp 1
      limitUp = true;
      limitDown = false; //
    }
  }
  if (buttons[1].rose() || limitDownPin == HIGH) { // limit switch 1st floor, turn off
    Serial.println("Limit Switch 1st floor");
    if (limitDown == false) {
      digitalWrite(RELAY1, LOW);
      digitalWrite(RELAY2, LOW);
      delay(500);
      // digitalWrite(LAMP2, LOW);   // Lamp 2      
      limitUp = false; //
      limitDown = true;
    }

  }
  if (buttons[2].rose() ) { // Relay 1 up
    Serial.println("Up Button");
    delay(500);
    limitUpPin = digitalRead(BUTTON_PINS[0]); // prevent
    Serial.print("Limit Up Pin: ");
    Serial.println(limitUpPin);
    if (limitUpPin == LOW) {       // if HIGH lift position is limit
      Serial.println("Relay1 ON");
      digitalWrite(RELAY2, LOW);
      // digitalWrite(LAMP1, HIGH);   // Lamp 1
      delay(3000);                 // delay for protect motor
      digitalWrite(RELAY1, HIGH);      
      limitUp = false;
      limitDown = true;  //
    }
  }
  if (buttons[3].rose() ) { // Relay 2 down
    Serial.println("Down Button");
    delay(500);
    limitDownPin = digitalRead(BUTTON_PINS[1]);
    Serial.print("Limit Down Pin: ");
    Serial.println(limitDownPin);
    if (limitDownPin == LOW) {
      Serial.println("Relay2 ON");      
      digitalWrite(RELAY1, LOW);
      // digitalWrite(LAMP2, HIGH);   // Lamp 2
      delay(3000);                 // delay for protect motor
      digitalWrite(RELAY2, HIGH);
      
      limitUp = true;  //
      limitDown = false;
    }
  }

  /*
  for (int i = 0; i < NUM_BUTTONS; i++)  {
    // Update the Bounce instance :
    buttons[i].update();
    // If it fell, flag the need to toggle the LED
    if ( buttons[i].fell() ) {
      needToToggleLed = true;
    }
  }

  // if a LED toggle has been flagged :
  if ( needToToggleLed ) {
    // Toggle LED state :
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
  */
  blink();
  // Reconnect if MQTT client is not connected.
  if (!mqttClient.connected()) 
  {
    reconnect();
  }
  mqttClient.loop();   // Call the loop continuously to establish connection to the server.
}

void blink()
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(LED_PIN, ledState);
  }
}

void readConfig()
{
  int saved;
  
  EEPROM.get(103, nodeID);
  EEPROM.get(112, token);
  EEPROM.get(500, saved);
  Serial.println();
  Serial.println("Configuration Read");
  Serial.print("Saved = ");
  Serial.println(saved);

  if (saved == 6550) {
    strcpy(c_token, token);
    ltoa(nodeID, c_nodeid, 10);

    const int n = snprintf(NULL, 0, "%lu", nodeID);
    assert(n > 0);
    char buf[n+1];
    int c = snprintf(buf, n+1, "%lu", nodeID);
    assert(buf[n] == '\0');
    assert(c == n);
    strcat(clientID, buf);

    Serial.print("Node ID : ");
    Serial.println(nodeID);
    Serial.print("Device token : ");
    Serial.println(token);
    Serial.print("Client ID : ");
    Serial.println(clientID);
  }
}


void setupWifi()
{
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  int saved = 6550;

  wifiManager.setBreakAfterConfig(true);
  wifiManager.setConfigPortalTimeout(120);

  WiFiManagerParameter custom_c_nodeid("c_nodeid", "MQTT Node ID", c_nodeid, 8);
  WiFiManagerParameter custom_c_token("c_token", "ThingsBoard Token", c_token, 21);
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_c_nodeid);
  wifiManager.addParameter(&custom_c_token);

  String alias = "ogoSwitchT-"+String(ESP.getChipId());
  if (!wifiManager.autoConnect(alias.c_str(), "")) {
    Serial.println("failed to connect and hit timeout");
    Alarm.delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    Alarm.delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  
  Serial.print("Device token : ");
  Serial.println(token);

  if (shouldSaveConfig) { //shouldSaveConfig
    Serial.println("Saving config...");
    strcpy(c_token, custom_c_token.getValue());
    strcpy(c_nodeid, custom_c_nodeid.getValue());

    strcpy(token, c_token);
    nodeID = (unsigned long) atol(c_nodeid);

    
    const int n = snprintf(NULL, 0, "%lu", nodeID);
    assert(n > 0);
    char buf[n+1];
    int c = snprintf(buf, n+1, "%lu", nodeID);
    assert(buf[n] == '\0');
    assert(c == n);
    strcat(clientID, buf);

    Serial.print("Node ID : ");
    Serial.println(nodeID);
    Serial.print("Device token : ");
    Serial.println(token);
    Serial.print("Client ID : ");
    Serial.println(clientID);

    EEPROM.put(103, nodeID);
    EEPROM.put(112, token);
    EEPROM.put(500, saved);
    
    if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
    } else {
      Serial.println("ERROR! EEPROM commit failed");
    }

    shouldSaveConfig = false;
  }
  
}


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupMqtt() 
{

  #ifdef THINGSBOARD
  mqttClient.setServer(thingsboardServer, mqttport);  // default port 1883, mqtt_server, thingsboardServer
  mqttClient.setCallback(callback);
  if (mqttClient.connect(clientID, token, NULL)) {
    mqttClient.subscribe("v1/devices/me/rpc/request/+");
    Serial.print("mqtt connected : ");    
    Serial.println(thingsboardServer);  // mqtt_server
    
  }
  #endif
  
  #ifdef MQTT
  Serial.print(mqttServer); Serial.print(" "); Serial.println(mqttport);
  mqttClient.setCallback(callback);
  mqttClient.setServer(mqttServer, mqttport);
  if (mqttClient.connect(clientID, mqttUserName, mqttPassword)) {
    mqttClient.subscribe( subscribeTopic.c_str() );
    Serial.print("mqtt connected : ");
        Serial.println(mqtt_server);  // mqtt_server
  }
  #endif
}

void reconnect() 
{
  
  // Loop until reconnected.
  // while (!mqttClient.connected()) {
  // }
    Serial.print("Attempting MQTT connection...");

    /*
    // Generate ClientID
    char clientID[9];
    for (int i = 0; i < 8; i++) {
        clientID[i] = alphanum[random(51)];
    }
    clientID[8]='\0';
    */
    
    // Connect to the MQTT broker
    #ifdef THINGSBOARD
    mqttClient.setServer(thingsboardServer, mqttport);
    if (mqttClient.connect(clientID, token, NULL)) {
      Serial.println();
      Serial.print("Connected with Client ID:  ");
      Serial.print(String(clientID));  
      Serial.print(", Token: ");
      Serial.println(token);
      
      // Subscribing to receive RPC requests
      mqttClient.subscribe("v1/devices/me/rpc/request/+");
    }
    else {

      Serial.print("failed, rc=");
      // Print to know why the connection failed.
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      Alarm.delay(2000);
      mqttCountReconnect++;
      if (mqttCountReconnect >= 10) {
        mqttCountReconnect = 0;
        Serial.println("Reset..");
        ESP.reset();
      }
    }
    
    #endif
    
    #ifdef MQTT
    mqttClient.setServer(mqtt_server, mqttport);
    if (mqttClient.connect(clientID,mqttUserName,mqttPassword)) {
      Serial.print("Connected with Client ID:  ");
      Serial.print(String(clientID));
      Serial.print(", Username: ");
      Serial.print(mqttUserName);
      Serial.print(" , Passwword: ");
      Serial.println(mqttPassword);  
      mqttClient.subscribe( subscribeTopic.c_str() );
    }
    else {
      Serial.print("failed, rc=");
      // Print to know why the connection failed.
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      Alarm.delay(2000);
      mqttCountReconnect++;
      if (mqttCountReconnect >= 10) {
        mqttCountReconnect = 0;
        Serial.println("Reset..");
        ESP.reset();
      }
    }
    #endif  
   
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  int i = 0;
  String responseTopic;
  String relayStatus;
  
  #ifdef THINGSBOARD

  Serial.println("On message");
  
  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(json);

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
  String valueName = String((const char *) data["params"]);

  if (methodName.equals("turnOff")) {
    turnOff();
    Serial.print("Relay " + valueName + " status: ");
    Serial.println(digitalRead(RELAY2) ? 1 : 0);
    
    relayStatus = String(digitalRead(RELAY2) ? 1 : 0, DEC);
    responseTopic = String(topic);
    
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), relayStatus.c_str());
  }
  else if (methodName.equals("turnOn")) {
    turnOn();
    Serial.print("Relay " + valueName + " status: ");
    Serial.println(digitalRead(RELAY1) ? 1 : 0);
    
    relayStatus = String(digitalRead(RELAY1) ? 1 : 0, DEC);
    responseTopic = String(topic);
    
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), relayStatus.c_str());
  }
  else if (methodName.equals("getValue")) {
    i = valueName.toInt();
    if (i < 1 || i > 2) {
      return;
    }
    Serial.print("Relay " + valueName + " status: ");
    if (i == 1) {
      relayStatus = String(digitalRead(RELAY1) ? 1 : 0, DEC);
    }
    else if (i == 2) {
      relayStatus = String(digitalRead(RELAY2) ? 1 : 0, DEC);
    }
    responseTopic = String(topic);
    
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), relayStatus.c_str());
    
    String message = "{\"Relay Status\":" + relayStatus + "}";
    mqttClient.publish("v1/devices/me/telemetry",  message.c_str());
    Serial.print("topic : ");
    Serial.print("v1/devices/me/telemetry");
    Serial.print(" : message ");
    Serial.println(message);
  }

  
  
  #endif
  
}

void turnOff()
{
  Serial.println("Down Button");
  delay(500);
  limitDownPin = digitalRead(BUTTON_PINS[1]);
  Serial.print("Limit Down Pin: ");
  Serial.println(limitDownPin);
  if (limitDownPin == LOW) {
    Serial.println("Relay2 ON");      
    digitalWrite(RELAY1, LOW);
    // digitalWrite(LAMP2, HIGH);   // Lamp 2
    delay(3000);                 // delay for protect motor
    digitalWrite(RELAY2, HIGH);
    
    limitUp = true;  //
    limitDown = false;
  }
}

void turnOn()
{
  Serial.println("Up Button");
  delay(500);
  limitUpPin = digitalRead(BUTTON_PINS[0]); // prevent
  Serial.print("Limit Up Pin: ");
  Serial.println(limitUpPin);
  if (limitUpPin == LOW) {       // if HIGH lift position is limit
    Serial.println("Relay1 ON");
    digitalWrite(RELAY2, LOW);
    // digitalWrite(LAMP1, HIGH);   // Lamp 1
    delay(3000);                 // delay for protect motor
    digitalWrite(RELAY1, HIGH);      
    limitUp = false;
    limitDown = true;  //
  }
}
