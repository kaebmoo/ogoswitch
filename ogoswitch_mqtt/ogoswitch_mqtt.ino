/*
MIT License

Copyright (c) 2017 kaebmoo

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

#include <PubSubClient.h>
#include "ESP8266WiFi.h"
#include <BlynkSimpleEsp8266.h>
#include <Time.h>
#include <TimeLib.h>
#include <Timer.h>

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).


// const char *ssid = "CAT-Mobile";
const char *ssid = "CAT-Register";
const char *password = "";
//const char* mqtt_server = "192.168.43.252";
const char* mqtt_server = "192.168.2.7";
//const char* mqtt_server = "192.168.8.50";
const int relayPin = D1;
const int statusPin = D2;
const int buzzer=D5; //Buzzer control port, default D5
const long interval = 1000;
int ledState = LOW;
unsigned long previousMillis = 0;

const int MAXRETRY=30; // 0 - 4

byte mac[6] { 0x60, 0x01, 0x94, 0x82, 0x85, 0x54};
IPAddress ip(192, 168, 9, 101);
IPAddress gateway(192, 168, 9, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress server(192, 168, 9, 1);

WiFiClient espClient;
PubSubClient client(espClient);

char *mqtt_user = "chang";
char *mqtt_password = "chang";

char *myRoom = "room/1";
char *room_status = "room/1/status";
char *room_start = "room/1/start";
char *room_stop = "room/1/stop";
char *room_currenttime = "room/1/currenttime";

int mqtt_reconnect = 0;
int wifi_reconnect = 0;
// int flagtime = 0;
boolean ON = false;
boolean bstart = false;
boolean bstop = false;
boolean bcurrent = false;
boolean force = false;
unsigned long starttime;
unsigned long stoptime;
unsigned long currenttime;
unsigned long topic_currenttime;
Timer t_settime;



void buzzer_sound()
{
  analogWriteRange(1047);
  analogWrite(buzzer, 512);
  delay(100);
  analogWrite(buzzer, 0);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  delay(100);

  analogWriteRange(1175);
  analogWrite(buzzer, 512);
  delay(300);
  analogWrite(buzzer, 0);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  delay(300);
}

void callback(char* topic, byte* payload, unsigned int length) {

  String strStart = "";
  String strStop = "";
  String strCurrenttime = "";

  int i;
  int statusvalue;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if(strcmp(topic, myRoom) == 0) {

    // Switch on the RELAY if an 1 was received as first character
    if ((char)payload[0] == '1') {
      relay(true);
      // force to open ; do not check start time, stop time
      force = true;
      bstart = false;
      bstop = false;

    } else if ((char)payload[0] == '0') {       // 0 - turn relay off
      // force to close ; do not check start time, stop time
      relay(false);
      force = true;
      bstart = false;
      bstop = false;

    } else if ((char)payload[0] == '2') {       // 2 - get status from D2 (connect D2 <---> D1) or D1 (relay pin)
        statusvalue = digitalRead(relayPin);
        if (statusvalue == HIGH) {
          client.publish(room_status, "ON");
      }
        else {
          client.publish(room_status, "OFF");
        }

    }
  }
  if (strcmp(topic, room_start) == 0) {
    // receive start time message
    for(i = 0; i < length; i++) {
      strStart += (char)payload[i];
    }

    starttime = strStart.toInt();
    strStart = "";

    bstart = true;
  }
  if (strcmp(topic, room_stop) == 0) {
    // receive stop time message
    for(i = 0; i < length; i++) {
      strStop += (char)payload[i];
    }
    stoptime = strStop.toInt();
    strStop = "";

    bstop = true;
  }

  if (bstart && bstop) {
    force = false;
  }

  if (strcmp(topic, room_currenttime) == 0) {
    //receive current time message
    for(i = 0; i < length; i++) {
      strCurrenttime += (char)payload[i];
    }

    topic_currenttime = strCurrenttime.toInt();
    strCurrenttime = "";
    if (timeStatus() != timeSet) {
      setTime((time_t) topic_currenttime);
      Serial.println("Time Set");
    }
    time_t t = now();
    Serial.print(hour(t));
    Serial.print(":");
    Serial.print(minute(t));
    Serial.print(":");
    Serial.print(second(t));
    Serial.print(" diff ");
    Serial.print(t - (time_t) topic_currenttime);
    Serial.println();

    if ( (t - (time_t) topic_currenttime) != 0) {
      setTime((time_t) topic_currenttime);
      Serial.println("Time Set");
    }

    bcurrent = true;

  }



}

void relay(boolean set)
{
  if (set) {
    ON = true;
    digitalWrite(relayPin, HIGH);
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)

    client.publish(room_status,"ON", true);
    Serial.print(room_status);
    Serial.println(" : ON");
    buzzer_sound();

  }
  else {
    ON = false;

    digitalWrite(relayPin, LOW);
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
    client.publish(room_status,"OFF", true);
    Serial.print(room_status);
    Serial.println(" : OFF");

    delay(500);
    digitalWrite(BUILTIN_LED, LOW);
    delay(500);
    digitalWrite(BUILTIN_LED, HIGH);

  }
}

void setupWifi() {

  WiFiManager wifiManager;
  String APName;

  delay(10);
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(300);


  APName = "OgoSwitch-"+String(ESP.getChipId());
  if(!wifiManager.autoConnect(APName.c_str()) ) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");



  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  Serial.print(mac[5],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.println(mac[0],HEX);

}

void wifiReconnect()
{

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println();
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.psk());

  //WiFi.config(ip,gateway,subnet);  // fix ip address
  WiFi.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifi_reconnect++;
    if (wifi_reconnect > MAXRETRY) {
      wifi_reconnect = 0;
      break;
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(myRoom, mqtt_user, mqtt_password)) {
      Serial.print("connected : ");
      Serial.println(mqtt_server);

      // Once connected, publish an announcement...
      client.publish(room_status, "hello world");

      // ... and resubscribe
      client.subscribe(myRoom);
      client.subscribe(room_start);
      client.subscribe(room_stop);
      client.subscribe(room_currenttime);

      buzzer_sound();
      buzzer_sound();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.print(" try : ");
      Serial.print(mqtt_reconnect+1);
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    mqtt_reconnect++;
    if (mqtt_reconnect > 5) {
      mqtt_reconnect = 0;

      break;
    }
  }
}

BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable

  // process received value
  Serial.println(pinValue);
  if (pinValue == 1)
    buzzer_sound();
}

void reconnectBlynk() {
  if (!Blynk.connected()) {
    if(Blynk.connect()) {
      BLYNK_LOG("Reconnected");
    } else {
      BLYNK_LOG("Not reconnected");
    }
  }
}

void checkvalidtime()
{
    Serial.print(bstart);
    Serial.print(" ");
    Serial.print(bstop);
    Serial.print(" ");
    Serial.print(bcurrent);
    Serial.print(" ");
    Serial.print(force);
    Serial.print(" ");
    Serial.print(starttime);
    Serial.print(" ");
    Serial.print(stoptime);
    Serial.print(" ");
    Serial.println(currenttime);

    //if (bstart && bstop && !bcurrent && !force) {
    //  setTime((time_t) starttime);
    //  bcurrent = true;
    //}

    if (bstart && bstop && bcurrent && !force) {
      if ( (currenttime >= starttime) && (currenttime <= stoptime) ) {
        if (!ON) {
          relay(true);
        }
      }
      else if (ON) {
        relay(false);
      }
    }
}

void takeSettingTime()
{
  setTime((time_t) topic_currenttime);
  Serial.println("Time Set");
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

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(statusPin, INPUT);
  pinMode(buzzer, OUTPUT);
  // t_settime.every(5000, takeSettingTime);

  Serial.println(myRoom);

  setupWifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  if (client.connect(myRoom, mqtt_user, mqtt_password)) {
    client.publish(room_status,"hello world");
    client.subscribe(myRoom);
    client.subscribe(room_start);
    client.subscribe(room_stop);
    client.subscribe(room_currenttime);
  }

  delay(500);

  //Blynk.config(auth);  // in place of Blynk.begin(auth, ssid, pass);
  //Blynk.connect(3333);  // timeout set to 10 seconds and then continue without Blynk, 3333 is 10 seconds because Blynk.connect is in 3ms units.

  //pinMode(BUILTIN_LED, OUTPUT);
  buzzer_sound();
  digitalWrite(BUILTIN_LED, LOW);  // turn on LED with voltage HIGH
  delay(200);                      // wait one second
  digitalWrite(BUILTIN_LED, HIGH);   // turn off LED with voltage LOW
  delay(200);

  time_t t = now();
  Serial.print("start : ");
  Serial.print(second(t));
  Serial.println();
}

void loop() {
  // put your main code here, to run repeatedly:

  if (WiFi.status() != WL_CONNECTED) {
    setupWifi();
  }
  else {
    if (!client.connected()) {
      reconnect();
    }
  }

  client.loop();
  //Blynk.run();

  //t_settime.update();
  currenttime = (unsigned long) now();
  checkvalidtime();
  if(!ON) {
    blink();
  }
  else {
    digitalWrite(BUILTIN_LED, HIGH);  // on LED D4 pin
  }
}
