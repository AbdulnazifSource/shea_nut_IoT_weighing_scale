#include "IoT_weighing_scale.h"

uint8_t dataPin = 26;
uint8_t clockPin = 25;


HX711 scale;

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;


float calibration_factor = 832.10; // for me this value works just perfect 419640 
float weight, price;
char blank[] = "         ";

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

int btnPin = 14;



BfButtonManager btnManager(btnPin, 4);
BfButton btn1(BfButton::ANALOG_BUTTON_ARRAY, 0, false, HIGH);
BfButton btn2(BfButton::ANALOG_BUTTON_ARRAY, 1, false, HIGH);
BfButton btn3(BfButton::ANALOG_BUTTON_ARRAY, 2, false, HIGH);
BfButton btn4(BfButton::ANALOG_BUTTON_ARRAY, 3, false, HIGH);

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
  Serial.print(btn->getID());
  switch (pattern)
  {
  case BfButton::SINGLE_PRESS:
    Serial.println(" pressed.");
      lcd.setCursor(7, 1);
      lcd.print(blank);
      lcd.setCursor(7, 1);
      lcd.print("pressed.");
    break;
  case BfButton::DOUBLE_PRESS:
    Serial.println(" double pressed.");
    lcd.setCursor(7, 1);
    lcd.print(blank);
    lcd.setCursor(7, 1);
    lcd.print("double P.");
    break;
  case BfButton::LONG_PRESS:
    Serial.println(" long pressed.");
    lcd.setCursor(7, 1);
    lcd.print(blank);
    lcd.setCursor(7, 1);
    lcd.print("long P.");
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(btnPin, INPUT_PULLDOWN);
  /*
  Serial.println(__FILE__);
  Serial.print("LIBRARY VERSION: ");
  Serial.println(HX711_LIB_VERSION);
  Serial.println();
  */ 
  //integrate event handlers to buttons and add buttons to event manager
  btn1.onPress(pressHandler)
      .onDoublePress(pressHandler)     // default timeout
      .onPressFor(pressHandler, 1000); // custom timeout for 1 second
  btnManager.addButton(&btn1, 3000, 3150);
  btn2.onPress(pressHandler)
      .onDoublePress(pressHandler)     // default timeout
      .onPressFor(pressHandler, 1000); // custom timeout for 1 second
  btnManager.addButton(&btn2, 2190, 2350);
  btn3.onPress(pressHandler)
      .onDoublePress(pressHandler)     // default timeout
      .onPressFor(pressHandler, 1000); // custom timeout for 1 second
  btnManager.addButton(&btn3, 1390, 1550);
  btn4.onPress(pressHandler)
      .onDoublePress(pressHandler)     // default timeout
      .onPressFor(pressHandler, 1000); // custom timeout for 1 second
  btnManager.addButton(&btn4, 600, 750);
  btnManager.begin();
  scale.begin(dataPin, clockPin);
  // initialize LCD
  lcd.begin();
  // turn on LCD backlight                      
  lcd.backlight();
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
  scale.tare();
  scale.set_scale(calibration_factor);
  lcd.clear();
}


void loop()
{

  btnManager.printReading(14);
  btnManager.loop();
  
  weight = scale.get_units(5);
  price = 20;
  
  lcd.setCursor(0, 0);
  lcd.print(weight);
  lcd.setCursor(0, 1);
  lcd.print(weight * price);
}
