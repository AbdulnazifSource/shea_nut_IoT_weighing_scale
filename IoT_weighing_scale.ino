#include "IoT_weighing_scale.h"

#define DOUT  26
#define CLK  25
HX711 scale(DOUT, CLK);
// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  
 
float weight; 
float calibration_factor = 419640; // for me this value works just perfect 419640 
void setup() {
  Serial.begin(9600);
  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  Serial.println("HX711 calibration sketch");
  Serial.println("Remove all weight from scale");
  Serial.println("After readings begin, place known weight on scale");
  Serial.println("Press + or a to increase calibration factor");
  Serial.println("Press - or z to decrease calibration factor");
  scale.set_scale();
  scale.tare(); //Reset the scale to 0
  long zero_factor = scale.read_average(); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);
}
void loop() {
  scale.set_scale(calibration_factor); //Adjust to this calibration factor
  weight = scale.get_units(1);
  //Serial.print(scale.get_units(), 2);
 // Serial.print(" lbs"); //Change this to kg and re-adjust the calibration factor if you follow SI units like a sane person
  
   // set cursor to first column, first row
  lcd.setCursor(0, 0);
  // print message
  lcd.clear();
  lcd.print("gram: ");
  lcd.print(weight);
  lcd.print(" g");
  
  // clears the display to print new message
  
  // set cursor to first column, second row
  lcd.setCursor(0,1);
  lcd.print("Calib: ");
  lcd.print(calibration_factor);
  if(Serial.available())
  {
    char temp = Serial.read();
    if(temp == '+' || temp == 'a')
      calibration_factor += 10;
    else if(temp == '-' || temp == 'z')
      calibration_factor -= 10;
  }
  delay(1000);
}
