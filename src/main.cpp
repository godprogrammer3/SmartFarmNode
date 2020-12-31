#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "SDL_ESP8266_HR_AM2315.h"
#include <IRremoteESP8266.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_ADS1015.h>
#define VERSION "1.0.13"

#define PIN_CONTROL_0 5
#define PIN_CONTROL_1 4
#define PIN_CONTROL_2 16
#define PIN_CONTROL_3 14
#define PIN_CONTROL_4 12
#define PIN_CONTROL_5 13

#define NEC_BITS 32
#define NEC_HDR_MARK 9000
#define NEC_HDR_SPACE 4500
#define NEC_BIT_MARK 560
#define NEC_ONE_SPACE 1690
#define NEC_ZERO_SPACE 560
#define NEC_RPT_SPACE 2250

/* Wifi config part */
const char *ssid = "SmartFarm2.4";
const char *password = "sf504504";
const char *mqtt_server = "192.168.1.11";
/********************/

/* Declare variable part */
WiFiClient espClient;
PubSubClient client(espClient);
String MAC_ADDRESS = "";

/*************************/

/* Declare function */
void wakeWiFi();
void reconnect();
void callback(char *topic, byte *payload, unsigned int length);
String parseValue(String str, int index);
void controlRemote(String command);
/*******************/

void setup()
{
  Serial.begin(115200);
  Serial.println();

  pinMode(PIN_CONTROL_0, OUTPUT);
  pinMode(PIN_CONTROL_1, OUTPUT);
  pinMode(PIN_CONTROL_2, OUTPUT);
  pinMode(PIN_CONTROL_3, OUTPUT);
  pinMode(PIN_CONTROL_4, OUTPUT);
  pinMode(PIN_CONTROL_5, OUTPUT);

  digitalWrite(PIN_CONTROL_0, 0);
  digitalWrite(PIN_CONTROL_1, 0);
  digitalWrite(PIN_CONTROL_2, 0);
  digitalWrite(PIN_CONTROL_3, 0);
  digitalWrite(PIN_CONTROL_4, 0);
  digitalWrite(PIN_CONTROL_5, 0);

  Serial.println();
  Serial.print("->Node V");
  Serial.println(VERSION);
  Serial.print("->MAC = ");
  MAC_ADDRESS = WiFi.macAddress();
  Serial.println(MAC_ADDRESS);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  wakeWiFi();
  reconnect();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    delay(1);
    wakeWiFi();
    return;
  }
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
}

/* wake WiFi part */
void wakeWiFi()
{
  Serial.print("Conecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  unsigned long long restartTimeOut = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - restartTimeOut >= 30000)
    {
      ESP.restart();
      delay(300);
    }
    Serial.print(".");
    delay(500);
  }
  delay(300);
}
/****************/

/* reconnect part */
void reconnect()
{
  // Loop until we're reconnected
  unsigned long long restartTimeOut = millis();
  while (!client.connected())
  {
    if (millis() - restartTimeOut >= 30000)
    {
      ESP.restart();
      delay(300);
    }
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8285Client-";
    clientId += MAC_ADDRESS;
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      String topic = "Node/";
      topic += MAC_ADDRESS;
      client.subscribe(topic.c_str());
      Serial.print("->Subscribe to [");
      Serial.print(topic);
      Serial.println("]");
      topic += "/#";
      client.subscribe(topic.c_str());
      Serial.print("->Subscribe to [");
      Serial.print(topic);
      Serial.println("]");
      client.subscribe("Node");
      Serial.println("->Subscribe to [Node]");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
      // Wait 3 seconds before retrying
      delay(3000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  String payload_str = "";
  String topic_str = topic;
  Serial.print("\n->Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    payload_str += (char)payload[i];
  }
  Serial.print("payload =  [");
  Serial.print(payload_str);
  Serial.println("] ");
  String argument[3];
  for (int i = 0; i < 3; i++)
  {
    argument[i] = parseValue(payload_str, i);
  }
  if (argument[0] == "set")
  {
    if (argument[1] == "config")
    {
      Serial.println("->Config command");
      if (argument[2] == "restart")
      {
        ESP.restart();
      }
      else
      {
        Serial.println("->Invalid command!");
      }
    }
    else if (argument[1] == "switch")
    {
      Serial.println("->Switch node command");
      int pin = topic_str.substring(topic_str.lastIndexOf("/") + 1).toInt();
      Serial.print("->port = ");
      Serial.println(pin);
      int pinOut;
      switch (pin)
      {
      case 0:
        pinOut = PIN_CONTROL_0;
        break;
      case 1:
        pinOut = PIN_CONTROL_1;
        break;
      case 2:
        pinOut = PIN_CONTROL_2;
        break;
      case 3:
        pinOut = PIN_CONTROL_3;
        break;
      case 4:
        pinOut = PIN_CONTROL_4;
        break;
      case 5:
        pinOut = PIN_CONTROL_5;
        break;
      default:
        break;
      }
      if (argument[2] == "1")
      {
        digitalWrite(pinOut, HIGH);
        Serial.println("->Turn on");
      }
      else
      {
        digitalWrite(pinOut, LOW);
        Serial.println("->Turn off");
      }
      Serial.println("->SwitchNode did command complete");
    }
    else
    {
      Serial.println("->Invalid command!");
    }
  }
  else
  {
    Serial.println("->Invalid command!");
  }
  // else if (topic_str.indexOf("ControlNode/AirConditioner") != -1)
  // {
  //   controlRemote(payload_str);
  //   Serial.print("->Set AirConditioner command ");
  //   Serial.print(payload_str);
  //   Serial.println(" completed");
  //   Serial.print("->Control node AirConditioner case");
  // }
}

String parseValue(String str, int index)
{
  String value = "";
  int cnt = 0;
  int runNow = 0;
  runNow = str.indexOf(';');
  while (cnt < index)
  {
    runNow = str.indexOf(';', runNow);
    runNow++;
    cnt++;
  }
  if (index == 0)
  {
    runNow = 0;
  }
  for (int i = runNow; i < str.indexOf(';', runNow + 1); i++)
  {
    value += str[i];
  }
  return value;
}
