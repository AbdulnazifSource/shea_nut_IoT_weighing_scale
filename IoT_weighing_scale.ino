#include "IoT_weighing_scale.h"

uint8_t dataPin = 32;
uint8_t clockPin = 33;

HX711_ADC LoadCell(dataPin, clockPin);

float weight, price;
char blank[] = "         ";
const int calVal_eepromAdress = 0;
unsigned long t = 0;




// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

// set button manager and initialize buttons
int btnPin = 34;
int numberOfbuttons = 4;
BfButtonManager btnManager(btnPin, numberOfbuttons);
BfButton btn1(BfButton::ANALOG_BUTTON_ARRAY, 0);
BfButton btn2(BfButton::ANALOG_BUTTON_ARRAY, 1);
BfButton btn3(BfButton::ANALOG_BUTTON_ARRAY, 2);
BfButton btn4(BfButton::ANALOG_BUTTON_ARRAY, 3);

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
  Serial.print(btn->getID());
  switch (pattern)
  {
    case BfButton::SINGLE_PRESS:
    Serial.println("Button pressed.");
    //LoadCell.tareNoDelay();
    LoadCell.tare();
    // check if last tare operation is complete:
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
    }
    break;
    case BfButton::DOUBLE_PRESS:
    Serial.println("Button double pressed.");
    //LoadCell.tareNoDelay();
    LoadCell.tare();
    // check if last tare operation is complete:
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
    }
    break;
    case BfButton::LONG_PRESS:
    Serial.println("Button Long pressed.");
    //LoadCell.tareNoDelay();
    LoadCell.tare();
    // check if last tare operation is complete:
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
    }
    break;
  }
}

void setup()
{
  Serial.begin(9600); //Serial.begin(57600);
  //rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
  delay(10);
  pinMode(btnPin, OUTPUT);
  Serial.println();
  Serial.println("Starting...");

  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  float calibrationValue; // calibration value (see example file "Calibration.ino")
  calibrationValue = 832.10; // uncomment this if you want to set the calibration value in the sketch
#if defined(ESP8266)|| defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
#endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom
  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
  
  //integrate event handlers to buttons and add buttons to event manager
  btn1.onPress(pressHandler)
      .onDoublePress(pressHandler)     // default timeout
      .onPressFor(pressHandler, 1000); // custom timeout for 1 second
  btnManager.addButton(&btn1, 200, 230);
  btn2.onPress(pressHandler)
      .onDoublePress(pressHandler)     // default timeout
      .onPressFor(pressHandler, 1000); // custom timeout for 1 second
  btnManager.addButton(&btn2, 490, 550);
  btn3.onPress(pressHandler)
      .onDoublePress(pressHandler)     // default timeout
      .onPressFor(pressHandler, 1000); // custom timeout for 1 second
  btnManager.addButton(&btn3, 750, 850);
  btn4.onPress(pressHandler)
      .onDoublePress(pressHandler)     // default timeout
      .onPressFor(pressHandler, 1000); // custom timeout for 1 second
  btnManager.addButton(&btn4, 900, 1100);
  btnManager.begin();
  
  lcd.begin();// initialize LCD
  lcd.backlight();// turn on LCD backlight
  lcd.clear();
  Serial.println("Startup is complete");

  /*
  Serial.print("UNITS: ");
  Serial.println(scale.get_units(10));

  Serial.println("\nEmpty the scale, press a key to continue");
  while(!Serial.available());
  while(Serial.available()) Serial.read();

  scale.tare();
  Serial.print("UNITS: ");
  Serial.println(scale.get_units(10));


  Serial.println("\nPut 1000 gr in the scale, press a key to continue");
  while(!Serial.available());
  while(Serial.available()) Serial.read();

  scale.calibrate_scale(1000, 5);
  Serial.print("UNITS: ");
  Serial.println(scale.get_units(10));

  Serial.println("\nScale is calibrated, press a key to continue");
  while(!Serial.available());
  while(Serial.available()) Serial.read();

  
  
  scale.set_unit_price(20);  // we only have one price
  lcd.setCursor(0, 0);
  lcd.print("UNITS: ");
  lcd.setCursor(0, 1);
  lcd.print(": ");
  */
  
}


void loop()
{
  //int z;
  //z = analogRead(btnPin);
  //if (z > 100) lcd.print(z);
  btnManager.printReading(btnPin);
  btnManager.loop();
  
  static boolean newDataReady = 0;
  const int serialPrintInterval = 100; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      Serial.print("Load_cell output val: ");
      Serial.println(i);
      lcd.clear();
      price = i * 20;
      
      //set display
      lcd.setCursor(0, 0);
      lcd.print("price: N");
      if(price < 0)
        lcd.print("0");
      else
        lcd.print(price, 1);
      lcd.setCursor(0, 1);
      if(i > 0)
        lcd.setCursor(1, 1);
      lcd.print(i, 1);
      lcd.print("g");

      newDataReady = 0;
      t = millis();
    }
  }
}

