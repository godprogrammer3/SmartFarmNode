#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "SDL_ESP8266_HR_AM2315.h"
#include <IRremoteESP8266.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_ADS1015.h>
#define VERSION "1.0.12"
#define PIN_CONTROL 14

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
unsigned long long loopTime[3];
String MAC_ADDRESS = "";
int sequenceNumber = 0;
SDL_ESP8266_HR_AM2315 am2315;
float dataAM2315[2]; //Array to hold data returned by sensor.  [0,1] => [Humidity, Temperature]
float tempBuffer = 0.0;
float humidBuffer = 0.0;
BH1750 lightMeter;
IRsend irsend(14);
Adafruit_ADS1115 ads;
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
  // put your setup code here, to run once:
  irsend.enableIROut(38);
  Serial.begin(115200);
  Serial.println();
  Wire.begin(4, 5);
  lightMeter.begin();
  ads.begin();
  pinMode(PIN_CONTROL, OUTPUT);
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
  loopTime[0] = millis();
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
  if (millis() - loopTime[0] >= 1000)
  {
    am2315.readData(dataAM2315);
    if (String(dataAM2315[1]) != "nan")
    {
      tempBuffer = dataAM2315[1];
    }
    if (String(dataAM2315[0]) != "nan")
    {
      humidBuffer = dataAM2315[0];
    }
    loopTime[0] = millis();
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
  if (argument[0] == "get")
  {
    if (argument[1] == "air_temp")
    {
      Serial.println("->Get air temp command");
      String value = String(tempBuffer) + ',' + argument[2];
      String topic = "Node/" + MAC_ADDRESS + "/value";
      client.publish(topic.c_str(), value.c_str());
      Serial.println("-> Publish to topic [" + topic + "] payload = [" + value + "]");
    }
    else if (argument[1] == "air_humid")
    {
      Serial.println("->Get air humid command");
      String value = String(humidBuffer) + ',' + argument[2];
      String topic = "Node/" + MAC_ADDRESS + "/value";
      client.publish(topic.c_str(), value.c_str());
      Serial.println("-> Publish to topic [" + topic + "] payload = [" + value + "]");
    }
    else if (argument[1] == "light")
    {
      Serial.println("->Get light command");
      String value = String(lightMeter.readLightLevel()) + ',' + argument[2];
      String topic = "Node/" + MAC_ADDRESS + "/value";
      client.publish(topic.c_str(), value.c_str());
      Serial.println("-> Publish to topic [" + topic + "] payload = [" + value + "]");
    }
    else if (argument[1] == "analog0")
    {
      Serial.println("->Get analog0 command");
      String value = String(ads.readADC_SingleEnded(0)) + ',' + argument[2];
      String topic = "Node/" + MAC_ADDRESS + "/value";
      client.publish(topic.c_str(), value.c_str());
      Serial.println("-> Publish to topic [" + topic + "] payload = [" + value + "]");
    }
    else if (argument[1] == "analog1")
    {
      Serial.println("->Get analog1 command");
      String value = String(ads.readADC_SingleEnded(1)) + ',' + argument[2];
      String topic = "Node/" + MAC_ADDRESS + "/value";
      client.publish(topic.c_str(), value.c_str());
      Serial.println("-> Publish to topic [" + topic + "] payload = [" + value + "]");
    }
    else if (argument[1] == "analog2")
    {
      Serial.println("->Get analog2 command");
      String value = String(ads.readADC_SingleEnded(2)) + ',' + argument[2];
      String topic = "Node/" + MAC_ADDRESS + "/value";
      client.publish(topic.c_str(), value.c_str());
      Serial.println("-> Publish to topic [" + topic + "] payload = [" + value + "]");
    }
    else if (argument[1] == "analog3")
    {
      Serial.println("->Get analog3 command");
      String value = String(ads.readADC_SingleEnded(3)) + ',' + argument[2];
      String topic = "Node/" + MAC_ADDRESS + "/value";
      client.publish(topic.c_str(), value.c_str());
      Serial.println("-> Publish to topic [" + topic + "] payload = [" + value + "]");
    }
    else
    {
      Serial.println("->Invalid command");
    }
  }
  else if (argument[0] == "set")
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
      if (argument[2] == "1")
      {
        digitalWrite(PIN_CONTROL, HIGH);
        Serial.println("->Turn on");
      }
      else
      {
        digitalWrite(PIN_CONTROL, LOW);
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
  // else if (topic_str.indexOf("SensorNode/TempAndHumid") != -1)
  // {
  //   tmpPayload = "";
  //   tmpPayload += String(tempBuffer);
  //   tmpPayload += ",";
  //   tmpPayload += String(humidBuffer);
  //   tmpTopic = oldTopic.substring(0, oldTopic.length() - 1) + "/response";
  //   client.publish(tmpTopic.c_str(), tmpPayload.c_str());
  //   Serial.print("->Publish topic = [");
  //   Serial.print(tmpTopic);
  //   Serial.print("]");
  //   Serial.print(" , ");
  //   Serial.print("Message = [");
  //   Serial.print(tmpPayload);
  //   Serial.println("]");
  //   Serial.print("->Sensor node TempAndHumid case");
  // }
  // else if (topic_str.indexOf("SensorNode/SoilMoisture") != -1)
  // {
  //   tmpPayload = "";
  //   tmpPayload = String(analogRead(A0));
  //   tmpTopic = oldTopic.substring(0, oldTopic.length()) + "/response";
  //   client.publish(tmpTopic.c_str(), tmpPayload.c_str());
  //   Serial.print("->Publish topic = [");
  //   Serial.print(tmpTopic);
  //   Serial.print("]");
  //   Serial.print(" , ");
  //   Serial.print("Message = [");
  //   Serial.print(tmpPayload);
  //   Serial.println("]");
  //   Serial.print("->Sensor node SoilMoisture case");
  // }
  // else if (topic_str.indexOf("SensorNode/light") != -1)
  // {
  //   tmpPayload = "";
  //   tmpPayload = String(lightMeter.readLightLevel());
  //   tmpTopic = oldTopic.substring(0, oldTopic.length()) + "/response";
  //   client.publish(tmpTopic.c_str(), tmpPayload.c_str());
  //   Serial.print("->Publish topic = [");
  //   Serial.print(tmpTopic);
  //   Serial.print("]");
  //   Serial.print(" , ");
  //   Serial.print("Message = [");
  //   Serial.print(tmpPayload);
  //   Serial.println("]");
  //   Serial.print("->Sensor node light case");
  // }
  // else if (topic_str.indexOf("ControlNode/AirConditioner") != -1)
  // {
  //   controlRemote(payload_str);
  //   Serial.print("->Set AirConditioner command ");
  //   Serial.print(payload_str);
  //   Serial.println(" completed");
  //   Serial.print("->Control node AirConditioner case");
  // }
}

//------------------------------ Control remote part --------------------------------
void controlRemote(String command)
{
  unsigned long data1 = strtoul(command.substring(0, 8).c_str(), NULL, 16);
  unsigned long data2 = strtoul(command.substring(8, 16).c_str(), NULL, 16);
  char data3 = strtoul(command.substring(16, 18).c_str(), NULL, 16);
  Serial.print("data1 = ");
  Serial.println(data1, HEX);

  Serial.print("data2 = ");
  Serial.println(data2, HEX);

  Serial.print("data3 = ");
  Serial.println(data3, HEX);
  /*uint32_t sendData = 0x00000200;
  int Total = temp - 15;
  sendData *= Total;
  data1+=sendData;*/
  // Header
  irsend.mark(NEC_HDR_MARK);
  irsend.space(NEC_HDR_SPACE);

  // Data
  for (uint32_t mask = 1UL << (32 - 1); mask; mask >>= 1)
  {
    // Serial.println("1");
    if (data1 & mask)
    {
      irsend.mark(NEC_BIT_MARK);
      irsend.space(NEC_ONE_SPACE);
    }
    else
    {
      irsend.mark(NEC_BIT_MARK);
      irsend.space(NEC_ZERO_SPACE);
    }
  }

  for (uint32_t mask = 1UL << (32 - 1); mask; mask >>= 1)
  {
    if (data2 & mask)
    {
      irsend.mark(NEC_BIT_MARK);
      irsend.space(NEC_ONE_SPACE);
    }
    else
    {
      irsend.mark(NEC_BIT_MARK);
      irsend.space(NEC_ZERO_SPACE);
    }
  }

  for (uint8_t mask = 1UL << (8 - 1); mask; mask >>= 1)
  {
    if (data3 & mask)
    {
      irsend.mark(NEC_BIT_MARK);
      irsend.space(NEC_ONE_SPACE);
    }
    else
    {
      irsend.mark(NEC_BIT_MARK);
      irsend.space(NEC_ZERO_SPACE);
    }
  }
  // Footer
  irsend.mark(NEC_BIT_MARK);
  irsend.space(0); // Always end with the LED off
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
