#include <WiFi.h>
// #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
// #include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <WiFiClient.h>
// #include <ESP8266mDNS.h>
// #include <ESP8266HTTPUpdateServer.h>
// #include <ESP8266HTTPClient.h>
// #include <ESP8266httpUpdate.h>

#include <EEPROM.h>
// #include <BlynkSimpleEsp8266.h>
#include <BlynkSimpleEsp32.h>
#include "mdns.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>

// #define BLYNKLOCAL
#define FARMLOCAL
#define THINGSBOARD

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "634021991b694e08b004ca8b13f08bc1";
char c_auth[33] = "634021991b694e08b004ca8b13f08bc1";           // authen token blynk

char thingsboardServer[40] = "box.greenwing.email";
int  mqttport = 1883;                      // 1883 or 1888
char token[32] = "qxqW3i0uQEkji1sZfmy0";   // device token from thingsboard server

char c_thingsboardServer[41] = "192.168.2.64";
char c_mqttport[8] = "1883";
char c_token[33] = "12345678901234567890";
char c_sendinterval[8] = "10000";

//flag for saving data
bool shouldSaveConfig = false;
int sendinterval = 60;                                       // send data interval time in second

#define TRIGGER_PIN 12                        // GPIO 
int _LED = 2;
const int analogReadPin1 = 32;               // read for set options Soil Moisture or else ...
const int analogReadPin2 = 33;               // read for set options Soil Moisture or else ...
const int analogReadPin3 = 35;               // read for set options Soil Moisture or else ...
const int RELAY1 = 25;                       // 5 = D1
const int RELAY2 = 26;                       // 
const int RELAY3 = 27;   
// 

#define GPIO0 16                            
#define GPIO2 17                             

#define GPIO0_PIN 11
#define GPIO2_PIN 12
// We assume that all GPIOs are LOW
boolean gpioState[] = {false, false};
const int MAXRETRY=5;

// soil moisture variables
int minADC = 0;                       // replace with min ADC value read in air
int maxADC = 3440;                     // replace with max ADC value read fully submerged in water
int soilMoistureSetPoint1 = 50;
int soilMoistureSetPoint2 = 50;
int soilMoistureSetPoint3 = 50;
int soilMoisture, mappedValue1, mappedValue2, mappedValue3;
int range1 = 20;
int range2 = 20;
int range3 = 20;

const long interval = 1000;
int ledState = LOW;
unsigned long previousMillis = 0;

int offline = 0;
BlynkTimer timer, timerStatus, timerCheckConnection;
WidgetLED led1(20);
WidgetLED led2(21);
WidgetLED led3(22);

char hostString[16] = {0};
WiFiClient client;
PubSubClient mqttClient(client);

char *mqtt_user = "";
char *mqtt_password = "";
const char* mqtt_server = "db.ogonan.com";
int mqtt_reconnect = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(_LED, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
   
  delay(1000);
  readConfig();
  wifiConnect();
  if (offline == 0) {
    #ifdef BLYNKLOCAL
    Blynk.config(auth, "blynk.ogonan.com", 80);  // in place of Blynk.begin(auth, ssid, pass);
    #endif
     
    #ifdef FARMLOCAL
    Blynk.config(auth, "192.168.2.64", 80);
    #endif
    
    #ifdef BLYNK
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
    #ifdef THINGSBOARD
    setup_mqtt();
    timer.setInterval(sendinterval * 1000, sendSoilMoistureData);
    #endif
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  #ifdef THINGSBOARD
  if ( !mqttClient.connected() ) {
    reconnect();
  }

  mqttClient.loop();
  #endif
  
  blink();

  if (Blynk.connected()) {
    Blynk.run();
  }
  timerCheckConnection.run();
  timerStatus.run();
  timer.run();
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

  if (mappedValue1 > (soilMoistureSetPoint1 + range1)) {
    Serial.println("High Moisture");
    Serial.println("Soil Moisture: Turn Relay Off");
    digitalWrite(RELAY1, LOW);
    led1.off();
  }
  else if (mappedValue1 < (soilMoistureSetPoint1 - range1)) {
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

  if (mappedValue2 > (soilMoistureSetPoint2 + range2)) {
    Serial.println("High Moisture");
    Serial.println("Soil Moisture: Turn Relay Off");
    digitalWrite(RELAY2, LOW);
    led2.off();
  }
  else if (mappedValue2 < (soilMoistureSetPoint2 - range2)) {
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

  if (mappedValue3 > (soilMoistureSetPoint3 + range3)) {
    Serial.println("High Moisture");
    Serial.println("Soil Moisture: Turn Relay Off");
    digitalWrite(RELAY3, LOW);
    led3.off();
  }
  else if (mappedValue3 < (soilMoistureSetPoint3 - range3)) {
    Serial.println("Low Moisture");
    Serial.println("Soil Moisture: Turn Relay On");
    digitalWrite(RELAY3, HIGH);
    led3.on();
  }
}

void sendSoilMoistureData()
{


  // Just debug messages
  Serial.println("Send soil moisture data.");
  Serial.print( "[ ") ;
  Serial.print( mappedValue1 );
  Serial.print( ", " );
  Serial.print( mappedValue2 );
  Serial.print( ", " );
  Serial.print( mappedValue3 );
  Serial.print( "]   -> " );

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"soil moisture 1\":";  payload += mappedValue1; payload += ",";
  payload += "\"soil moisture 2\":";  payload += mappedValue2; payload += ",";
  payload += "\"soil moisture 3\":";  payload += mappedValue3; payload += ",";
  #ifdef SLEEP
  float volt = checkBattery();
  payload += "\"battery\":"; payload += volt; payload += ",";
  #endif
  payload += "\"active 1\":"; payload += digitalRead(RELAY1) ? true : false; payload += ",";
  payload += "\"active 2\":"; payload += digitalRead(RELAY2) ? true : false; payload += ",";
  payload += "\"active 3\":"; payload += digitalRead(RELAY3) ? true : false;
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
    digitalWrite(_LED, ledState);
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
    if ( digitalRead(TRIGGER_PIN) == HIGH ) {
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
  Serial.println();
  if (offline == 0) {    
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void ondemandWiFi()
{
  WiFiManager wifiManager;

  WiFiManagerParameter custom_c_auth("c_auth", "Auth Token", c_auth, 37);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_c_auth);
  delay(10);

  if (!wifiManager.startConfigPortal("ogosense-SoilWatch")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
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

void setup_mqtt()
{
  #ifdef THINGSBOARD
    #ifndef FARMLOCAL
    Serial.println("FARM Server : "+String(thingsboardServer)+" mqtt port: "+String(mqttport));
    mqttClient.setServer(thingsboardServer, mqttport);  // default port 1883, mqtt_server, thingsboardServer
    #else
    Serial.println("FARM Server : "+String("192.168.2.64")+" mqtt port: "+String(mqttport));
    mqttClient.setServer("192.168.2.64", mqttport);  // default port 1883, mqtt_server, thingsboardServer
    #endif    
  #else
  mqttClient.setServer(mqtt_server, mqttport);
  #endif
  mqttClient.setCallback(callback);
  if (mqttClient.connect("soilmoisture-esp32", token, NULL)) {
    Serial.print("mqtt connected : ");
    Serial.println();  
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
    if (mqttClient.connect("soilmoisture-esp32", token, NULL)) {  // connect to thingsboards
    #else
    if (mqttClient.connect("soilmoisture-esp32", mqtt_user, mqtt_password)) {  // connect to thingsboards
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
  Blynk.syncVirtual(V3);
  Blynk.syncVirtual(V4);
  Blynk.syncVirtual(V5);
  Blynk.syncVirtual(V6);
}

BLYNK_WRITE(V1)
{
  soilMoistureSetPoint1 = param.asInt();
  Serial.print("Set point: ");
  Serial.println(soilMoistureSetPoint1);
  Serial.println();

  Blynk.virtualWrite(V31, soilMoistureSetPoint1+range1);
  Blynk.virtualWrite(V32, soilMoistureSetPoint1-range1);
}

BLYNK_WRITE(V2)
{
  range1 = param.asInt();
  Serial.print("Range: ");
  Serial.println(range1);
  Serial.println();

  Blynk.virtualWrite(V31, soilMoistureSetPoint1+range1);
  Blynk.virtualWrite(V32, soilMoistureSetPoint1-range1);
}

BLYNK_WRITE(V3)
{
  soilMoistureSetPoint2 = param.asInt();
  Serial.print("Set point: ");
  Serial.println(soilMoistureSetPoint2);
  Serial.println();

  Blynk.virtualWrite(V33, soilMoistureSetPoint2+range2);
  Blynk.virtualWrite(V34, soilMoistureSetPoint2-range2);
}

BLYNK_WRITE(V4)
{
  range2 = param.asInt();
  Serial.print("Range: ");
  Serial.println(range2);
  Serial.println();

  Blynk.virtualWrite(V33, soilMoistureSetPoint2+range2);
  Blynk.virtualWrite(V34, soilMoistureSetPoint2-range2);
}

BLYNK_WRITE(V5)
{
  soilMoistureSetPoint3 = param.asInt();
  Serial.print("Set point: ");
  Serial.println(soilMoistureSetPoint3);
  Serial.println();

  Blynk.virtualWrite(V35, soilMoistureSetPoint3+range3);
  Blynk.virtualWrite(V36, soilMoistureSetPoint3-range3);
}

BLYNK_WRITE(V6)
{
  range3 = param.asInt();
  Serial.print("Range: ");
  Serial.println(range3);
  Serial.println();

  Blynk.virtualWrite(V35, soilMoistureSetPoint3+range3);
  Blynk.virtualWrite(V36, soilMoistureSetPoint3-range3);
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
