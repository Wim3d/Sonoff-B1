/*
  Written by W.J. Hoogervorst
  Firmware for Sonoff-B1 with Openhab integration and Web interface
  March 2019
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <credentials.h>
#include "my92xx.h"

// for HTTPupdate
const char* host = "Sonoff_B1";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
const char* software_version = "version 5";

// MQTT
WiFiClient espClient;
PubSubClient client(espClient);
const char* mqtt_id = "Sonoff_B1";
const char* RGB_topic = "Sonoff_B1/RGB";
const char* DIM_topic = "Sonoff_B1/DIM";
const char* CW_topic = "Sonoff_B1/CW";
const char* WW_topic = "Sonoff_B1/WW";

// For the Led driver
#define MY92XX_MODEL        MY92XX_MODEL_MY9231   // the Sonoff-B1 uses a MY9231
#define MY92XX_CHIPS        2                     // two MY9231 chips 
#define MY92XX_DI_PIN       12                    // DI pin attached to GPIO12 of ESP8266
#define MY92XX_DCKI_PIN     14                    // DCKI pin attached to GPIO14 of ESP8266

#define MY92XX_RED          4                     // Red led attached to pin 4 of MY9231 set
#define MY92XX_GREEN        3                     // Green led attached to pin 3 of MY9231 set
#define MY92XX_BLUE         5                     // Blue led attached to pin 5 of MY9231 set
#define MY92XX_CW           0                     // Cold white led attached to pin 0 of MY9231 set
#define MY92XX_WW           1                     // warm white led attached to pin 1 of MY9231 set

#define WIFI_CONNECT_TIMEOUT_S 15

my92xx * _my92xx;

int scene = 0;
int state = 0;
int scenespeed = 10;

// initialize led values
String greenLedVal = "0";
String redLedVal = "0";
String blueLedVal = "0";
String WWLedVal = "0";
String CWLedVal = "0";

int redLedValint = 0, greenLedValint = 0, blueLedValint = 0, CWLedValint = 0;
int WWLedValint = 100;  // start with WW light on as a normal bulb

float brightness = 100;

#define SERIAL_DEBUG 0

uint32_t time1;

void updateLED(void) {
  _my92xx->setChannel(MY92XX_RED, redLedValint * 2.55 * (brightness / (float)100)); // from 100 to 255
  _my92xx->setChannel(MY92XX_GREEN, greenLedValint * 2.55 * (brightness / (float)100)); // from 100 to 255
  _my92xx->setChannel(MY92XX_BLUE, blueLedValint * 2.55 * (brightness / (float)100)); // from 100 to 255
  _my92xx->setChannel(MY92XX_CW, CWLedValint * 2.55 * (brightness / (float)100)); // from 100 to 255
  _my92xx->setChannel(MY92XX_WW, WWLedValint * 2.55 * (brightness / (float)100)); // from 100 to 255
  _my92xx->update();
}

void setup() {
  // if serial is not initialized all following calls to serial end dead.
  if (SERIAL_DEBUG)
  {
    Serial.begin(115200);
    delay(10);
    Serial.println("");
    Serial.println("");
    Serial.println("");
    Serial.println(F("Started from reset"));
  }

  // MY9291 with 4 channels (like the AiThinker Ai-Light)
  _my92xx = new my92xx(MY92XX_MODEL, MY92XX_CHIPS, MY92XX_DI_PIN, MY92XX_DCKI_PIN, MY92XX_COMMAND_DEFAULT);
  _my92xx->setState(true);
  updateLED();
  WiFi.mode(WIFI_STA);

  setup_wifi();

  // for HTTPudate
  MDNS.begin(host);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  httpServer.on("/", handleRoot);
  httpServer.on("/power_on", handle_power_on);
  httpServer.on("/power_off", handle_power_off);
  httpServer.on("/red", handle_red);
  httpServer.on("/green", handle_green);
  httpServer.on("/blue", handle_blue);
  httpServer.on("/dim10", handle_dim10);
  httpServer.on("/dim50", handle_dim50);
  httpServer.on("/dim100", handle_dim100);
  httpServer.on("/scene1", handle_scene1);
  httpServer.on("/scene2", handle_scene2);
  httpServer.on("/scene3", handle_scene3);
  httpServer.onNotFound(handle_NotFound);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  if (!client.connected()) {
    reconnect();
  }
  client.publish("Sonoff_B1/status", "Sonoff_B1 powered on");
}


void loop() {
  client.loop();
  httpServer.handleClient();    // for HTTPupdate
  delay(10);
  if (!client.connected())
  {
    reconnect();
  }
  switch (scene)
  {
    case 0: break;
    case 1:
      {
        scenespeed = 10;
        scene1();
        break;
      }
    case 2:
      {
        scenespeed = 5;
        scene1();
        break;
      }
    case 3:
      {
        scenespeed = 1;
        scene1();
        break;
      }
  }
}

void scene1(void)
{
  switch (state)
  {
    case 0:
      {
        redLedValint = 100;
        greenLedValint++;
        if (greenLedValint == 100)
          state++;
        break;
      }

    case 1:
      {
        redLedValint--;
        if (redLedValint == 0)
          state++;
        break;
      }
    case 2:
      {
        blueLedValint++;
        if (blueLedValint == 100)
          state++;
        break;
      }
    case 3:
      {
        greenLedValint--;
        if (greenLedValint == 0)
            state++;
            break;
      }
  case 4:
    {
      redLedValint++;
      if (redLedValint == 100)
          state++;
        break;
      }
    case 5:
      {
        blueLedValint--;
        if (blueLedValint == 0)
          state++;
        break;
      }
    case 6:
      {
        state = 0;
        break;
      }
  }
  updateLED();
  delay(scenespeed);
}

void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    //payload2[i] = payload[i];
  }
  Serial.println();
  Serial.print("length: ");
  Serial.print(length);

  String stringOne = (char*)payload;
  Serial.println();
  Serial.print("StringOne: ");
  Serial.println(stringOne);

  if ((char)topic[10] == 'D') // search for   Sonoff_B1/DIM
  {
    String sbrightness = stringOne.substring(0, length);
    brightness = sbrightness.toInt();
  }
  else
  {
    redLedValint = 0;
    greenLedValint = 0;
    blueLedValint = 0;
    WWLedValint = 0;
    CWLedValint = 0;

    if ((char)topic[10] == 'R') // search for   Sonoff_B1/RGB
    {
      scene = 0;

      // Serial.println(stringOne);
      //Serial.println();
      // find first ; in the string
      int end_of_redvalue = stringOne.indexOf(',') - 1;

      // find second ; in the string
      int start_of_greenvalue = end_of_redvalue + 2;
      int end_of_greenvalue = stringOne.indexOf(',', start_of_greenvalue) - 1;

      // find the third ; in the string
      int start_of_bluevalue = end_of_greenvalue + 2;
      
      // using the locations of ; find values
      redLedVal = stringOne.substring(0, (end_of_redvalue + 1));
      greenLedVal = stringOne.substring(start_of_greenvalue, (end_of_greenvalue + 1));
      blueLedVal = stringOne.substring(start_of_bluevalue, length);

      redLedValint = redLedVal.toInt();
      greenLedValint = greenLedVal.toInt();
      blueLedValint = blueLedVal.toInt();
    }
    else if ((char)topic[10] == 'W') // search for   Sonoff_B1/WW
    {
      if (length <= 3)  // if length is too long, it is no value but another message
      {
        scene = 0;
        WWLedVal = stringOne.substring(0, length);
        WWLedValint = WWLedVal.toInt();
        CWLedValint = 100 - WWLedValint;
      }
    }
    else if ((char)topic[10] == 'C') // search for   Sonoff_B1/WW
    {
      if (length <= 3)
      {
        scene = 0;
        CWLedVal = stringOne.substring(0, length);
        CWLedValint = CWLedVal.toInt();
        WWLedValint = 100 - CWLedValint;
      }
    }
    else if ((char)topic[11] == 'c') // search for   Sonoff_B1/scene#
    {
      state = 0;
      scene = (char)topic[15] - '0';
    }
  }
  Serial.print("Brightness: ");
  Serial.println(brightness);
  Serial.print("redLedValint: ");
  Serial.print(redLedValint);
  Serial.print(".  Red corrected for brightness: ");
  Serial.println(redLedValint * 2.55 * (brightness / (float)100));
  Serial.print("greenLedValint: ");
  Serial.println(greenLedValint);
  Serial.println(greenLedValint * 2.55 * ((float)brightness / (float)100));
  Serial.print("blueLedValint: ");
  Serial.println(blueLedValint);
  Serial.print("WWLedValint: ");
  Serial.println(WWLedValint);
  Serial.print("CWLedValint: ");
  Serial.println(CWLedValint);
  Serial.print("scene: ");
  Serial.println(scene);

  updateLED();
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(mySSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(mySSID, myPASSWORD);
  time1 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    Serial.print(".");
    if (millis() > time1 + (WIFI_CONNECT_TIMEOUT_S * 1000))
      ESP.restart();
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println(WiFi.localIP());
}

boolean reconnect()
{
  if (WiFi.status() != WL_CONNECTED) {    // check if WiFi connection is present
    setup_wifi();
  }
  Serial.println("Attempting MQTT connection...");
  if (client.connect(host)) {
    Serial.println("connected");
    // ... and resubscribe
    client.subscribe(RGB_topic);
    client.subscribe(WW_topic);
    client.subscribe(CW_topic);
    client.subscribe(DIM_topic);
    client.subscribe("Sonoff_B1/scene1");
    client.subscribe("Sonoff_B1/scene2");
    client.subscribe("Sonoff_B1/scene3");
    client.publish("Sonoff_B1/status", "Sonoff_B1 reconnected");
  }
  Serial.println(client.connected());
  return client.connected();
}

void handleRoot() {
  Serial.println("Connected to client");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_power_off() {
  client.publish(RGB_topic, "0,0,0");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_power_on() {
  client.publish(WW_topic, "100");
  brightness = 100;
  httpServer.send(200, "text/html", SendHTML());
}

void handle_red() {
  client.publish(RGB_topic, "100,0,0");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_green() {
  client.publish(RGB_topic, "0,100,0");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_dim100() {
  client.publish(DIM_topic, "100");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_dim50() {
  client.publish(DIM_topic, "50");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_dim10() {
  client.publish(DIM_topic, "10");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_blue() {
  client.publish(RGB_topic, "0,0,100");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_WW() {
  client.publish(WW_topic, "100");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_CW() {
  client.publish(CW_topic, "100");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_scene1() {
  client.publish("Sonoff_B1/scene1", "ON");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_scene2() {
  client.publish("Sonoff_B1/scene2", "ON");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_scene3() {
  client.publish("Sonoff_B1/scene3", "ON");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_NotFound() {
  httpServer.send(404, "text/plain", "Not found");
}


String SendHTML() {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>";
  ptr += host;
  ptr += "</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 25px auto 30px;} h3 {color: #444444;margin-bottom: 20px;}\n";
  ptr += ".button {width: 150px;background-color: #1abc9c;border: none;color: white;padding: 13px 10px;text-decoration: none;font-size: 20px;margin: 0px auto 15px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-red {background-color: #ff0000;}\n";
  ptr += ".button-red:active {background-color: #ea0000;}\n";
  ptr += ".button-green {background-color: #00ff00;}\n";
  ptr += ".button-green:active {background-color: #00ea00;}\n";
  ptr += ".button-blue {background-color: #0000ff;}\n";
  ptr += ".button-blue:active {background-color: #0000ea;}\n";
  ptr += ".button-dim {background-color: #0088ea;}\n";
  ptr += ".button-dim:active {background-color: #0072c4;}\n";
  ptr += ".button-pwr {background-color: #34495e;}\n";
  ptr += ".button-pwr:active {background-color: #2c3e50;}\n";
  ptr += ".button-update {background-color: #a32267;}\n";
  ptr += ".button-update:active {background-color: #fa00ff;}\n";
  ptr += ".button-scene {background-color: #d700db;}\n";
  ptr += ".button-scene:active {background-color: #961f5f;}\n";
  ptr += "p {font-size: 18px;color: #383535;margin-bottom: 15px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Sonoff B1 Web Control</h1>\n";
  ptr += "<h3>Control of Lights and link to HTTPWebUpdate</h3>\n";
  ptr += "<p>WimIOT\nDevice: ";
  ptr += host;
  ptr += "<br>Software version: ";
  ptr += software_version;
  ptr += "<br><br></p>";

  ptr += "<a class=\"button button-pwr\" href=\"/power_on\">Power ON</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-pwr\" href=\"/power_off\">Power OFF</a><br><br><br>\n";
  ptr += "<a class=\"button button-dim\" href=\"/dim10\">Dim 10%</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-dim\" href=\"/dim50\">Dim 50%</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-dim\" href=\"/dim100\">Dim 100%</a><br><br><br><br>\n";
  ptr += "<a class=\"button button-red\" href=\"/red\">Red</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-green\" href=\"/green\">Green</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-blue\" href=\"/blue\">Blue</a><br><br>\n";
  ptr += "<p>Scene</p>";
  ptr += "<a class=\"button button-scene\" href=\"/scene1\">Scene 1</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-scene\" href=\"/scene2\">Scene 2</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-scene\" href=\"/scene3\">Scene 3</a><br><br><br>\n";
  //  ptr += "<br><br></p>\n";
  ptr += "<p>Click for update page</p><a class =\"button button-update\" href=\"/update\">Update</a>\n";

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
