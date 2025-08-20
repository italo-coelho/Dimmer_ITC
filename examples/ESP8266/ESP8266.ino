#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>

#include "Dimmer_ITC.h"

const char *name = "Dimmer";

const int LED   =  4;
const int ZC    = 12;
const int TRIG  = 14;

// Objects
ESP8266WebServer server(80);
Dimmer_ITC dimmer(ZC, TRIG);

// Global Variables
uint8_t level = 0;

// Function Prototypes
void statusLED();


void setup()
{
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  Serial.begin(115200);
  Serial.print("\nHi there! My name is ");
  Serial.println(name);

  dimmer.begin();
  dimmer.calibrate();

  EEPROM.begin(16);
  if(EEPROM.read(0) != 'd')
  {
    EEPROM.write(0, 'd');
    EEPROM.write(1, 0);
    EEPROM.commit();
  }
  level = EEPROM.read(1);

  dimmer.setLevel(level);

  WiFiManager wifi;
  wifi.setHostname(name);
  wifi.setDarkMode(true);
  wifi.autoConnect(name);

  server.on("/frequency", []()
  {
    server.send(200, "text/html", "[" + String(name) + "]: " + String(dimmer.getFrequency()) + "Hz\n");
  });
  server.on("/level", []()
  {
    int lvl = server.arg(0).toInt();
    if(lvl >= 0 && lvl <= 255)
    {
      server.send(200, "text/html", "[" + String(name) + "]: OK\n");
      dimmer.setLevel(lvl);
    }
    else
    {
      server.send(200, "text/html", "[" + String(name) + "]: Invalid Number! Level must be [0,255]\n");
    }
  });
  server.on("/on", []()
  {
    dimmer.setLevel(255);
    server.send(200, "text/html", "[" + String(name) + "]: OK\n");
  });
  server.on("/off", []()
  {
    dimmer.setLevel(0);
    server.send(200, "text/html", "[" + String(name) + "]: OK\n");
  });
  server.on("/calibration", []()
  {
    server.send(200, "text/html", "[" + String(name) + "]: " + String(dimmer.getCalibration()) + "\n");
  });
  server.begin();

  ArduinoOTA.setHostname(name);
  ArduinoOTA.begin();

  MDNS.begin(name);
  MDNS.addService("http", "tcp", 80);
}

void loop() 
{
  statusLED();

  if(WiFi.status() == WL_CONNECTED)
  {
    MDNS.update();
    ArduinoOTA.handle();
    server.handleClient();
  }
}


void statusLED()
{
  byte blynks = 1;
  if((fabs(dimmer.getFrequency() - 50) <= FREQ_VAR) || (fabs(dimmer.getFrequency() - 60) <= FREQ_VAR))
    blynks += 1;
#ifndef SERIAL_ONLY
  if(WiFi.status() == WL_CONNECTED)
    blynks += 2;
#endif //!SERIAL_ONLY

  static byte i = 0;
  static uint32_t then1 = millis();
  static uint32_t then2 = millis();
  if(millis() - then1 >= 1000)
  {
    if(i < 2*blynks)
    {
      if(millis() - then2 >= 200)
      {
        digitalWrite(LED, !digitalRead(LED));
        then2 = millis();
        i++;
      }
    }
    else
      then1 = millis();
  }
  else
    i = 0;
}