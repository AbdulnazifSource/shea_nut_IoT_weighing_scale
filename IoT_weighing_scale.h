#ifndef IOT_WEIGHING_SCALE
#define IOT_WEIGHING_SCALE
#include <Arduino.h>
#include "src/LiquidCrystal_I2C/LiquidCrystal_I2C.h"
#include "src/HX711_ADC/HX711_ADC.h"
#include "src/ButtonFever/BfButton.h"
#include "src/ButtonFever/BfButtonManager.h"
#include "src/ArduinoJson/ArduinoJson.h"
#include "src/EasyBuzzer/EasyBuzzer.h"
#include "src/singleLEDLibrary/singleLEDLibrary.h"
//#include "soc/rtc.h"
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include "EEPROM.h"
#endif
#endif