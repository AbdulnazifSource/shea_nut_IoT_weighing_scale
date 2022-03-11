#ifndef IOT_WEIGHING_SCALE
#define IOT_WEIGHING_SCALE
#include <Arduino.h>
#include <HardwareSerial.h>
#include "src/StreamDebugger/StreamDebugger.h"
#include "src/LiquidCrystal_I2C/LiquidCrystal_I2C.h"
#include "src/HX711_ADC/HX711_ADC.h"
#include "src/ButtonFever/BfButton.h"
#include "src/ButtonFever/BfButtonManager.h"
//Please enter your CA certificate in ca_cert.h
#include "src/ca/ca_cert.h"
#include "src/SSLClient/SSLClient.h"
#include "src/ArduinoHttpClient/ArduinoHttpClient.h"
// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
// Include after TinyGSM definitions
#include "src/TinyGsmClient/TinyGsmClient.h"
//#include "soc/rtc.h"
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include "EEPROM.h"
#endif
#endif