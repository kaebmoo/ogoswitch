/*
  MIT License
Version 1.0 2020-1-9
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

#include <EEPROM.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <Wire.h>
#include "Adafruit_MCP23008.h"
#include <Arduino.h>
#include "Timer.h"
#include <ThingSpeak.h>
#include <ArduinoJson.h>
// #include <MicroGear.h>
#include <MQTTClient.h>
#include <PubSubClient.h>

WiFiClient switchClient;
Timer timer;

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 432257;
char *writeAPIKey = "";
char *readAPIKey = "";

// MicroGear microgear(switchClient);              // declare microgear object
PubSubClient mqttClient(switchClient);

char *myRoom = "switch/trakool/1/8";
int mqtt_reconnect = 0;

#define TOKEN "vAGJcv4MwAenW8ohYXrI"                        // device token 
#define MQTTPORT  1883                                      // 1883 or 1888
#define SENDINTERVAL  60000                                 // send data interval time

char token[32] = "99999999999999999999";                    // device token from thingsboard server
int mqttport = 1883;
char thingsboardServer[40] = "thingsboard.ogonan.com";      // "box.greenwing.email" "192.168.2.64"
int sendinterval = 60000;                                   // send data interval time ms

unsigned long lastMqttConnectionAttempt = 0;
unsigned long lastSend;
const int MAXRETRY=30;

char c_thingsboardServer[41] = "192.168.1.10";
char c_mqttport[8] = "1883";
char c_token[33] = "12345678901234567890";
char c_sendinterval[8] = "10000";                           // ms

int wifi_reconnect = 0;
bool shouldSaveConfig = false;

Adafruit_MCP23008 mcp;

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  EEPROM.begin(512);

  // mcp.begin(1); // address = 0 (valid: 0-7)
  setup_relayboard(0);
  while (!Serial); // wait for serial port to connect. Needed for Leonardo only
  Serial.println("I2C Relayboard test - press keys 12345678 (toggle relay) C (clear all)");
  relay_reset();
  delay(2000);
}

void loop() {
  // put your main code here, to run repeatedly:

}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup_relayboard(int board)
{
  mcp.begin(board);
  mcp.writeGPIO(0); // set OLAT to 0
  mcp.pinMode(0, OUTPUT); // set IODIR to 0
  mcp.pinMode(1, OUTPUT);
  mcp.pinMode(2, OUTPUT);
  mcp.pinMode(3, OUTPUT);
  mcp.pinMode(4, OUTPUT);
  mcp.pinMode(5, OUTPUT);
  mcp.pinMode(6, OUTPUT);
  mcp.pinMode(7, OUTPUT);
}

void relay_reset()
{
  int i;

  for (i = 0; i < 8; i++) {
    mcp.digitalWrite(i, LOW);
  }
  delay(500);
  for (i = 0; i < 8; i++) {
    Serial.print(i+1);
    Serial.print(mcp.digitalRead(i) ? ": ON  " : ": OFF ");
  }
  Serial.println();
}
