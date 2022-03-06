#include "IoT_weighing_scale.h"

uint8_t dataPin = 23;
uint8_t clockPin = 19;

HX711_ADC LoadCell(dataPin, clockPin);

float price;
const int calVal_eepromAdress = 0;
unsigned long t = 0;

// sim module
int simRXPin = 16;
int simTXPin = 17;
int simResetPin = 18;

// create an instance for serial communication to sim800l module
//HardwareSerial sim800l(2);

// Set serial for debug console (to the Serial Monitor)
#define SerialMon Serial
// Set serial for AT commands (to the SIM800 module)
#define SerialAT Serial2

const char apn[] = "web.gprs.mtnnigeria.net";       // Your APN
const char gprs_user[] = "web"; // User
const char gprs_pass[] = "web"; // Password
const char simPIN[] = "";    // SIM card PIN code, if any

const char hostname[] = "https://reqres.in"; // www.howsmyssl.com
const char resource[] = "/api/users?page=2";
int port = 443;

// Layers stack
TinyGsm sim_modem(SerialAT);
TinyGsmClient gsm_transpor_layer(sim_modem);
SSLClient secure_presentation_layer(&gsm_transpor_layer);
HttpClient http_client = HttpClient(secure_presentation_layer, hostname, port);

// buzzer
int buzzerPin = 32;

// processing indicator LED
int processPin = 33;

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// reset button
int resetPin = En;

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

void httpRequestHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
  Serial.print(btn->getID());
  if (pattern == BfButton::SINGLE_PRESS) {
    Serial.println(" button pressed.");
    // HTTP Test
    if (sim_modem.isGprsConnected()) {
      Serial.println("");
      Serial.println("Making GET request");
      http_client.get(resource);

      int status_code = http_client.responseStatusCode();
      String response = http_client.responseBody();

      Serial.print("Status code: ");
      Serial.println(status_code);
      Serial.print("Response: ");
      Serial.println(response);

      http_client.stop();
    } else {
      Serial.println("...not connected");
    }

    // Disconnect GPRS
    //sim_modem.gprsDisconnect();
    //SerialMon.println("GPRS disconnected");
    delay(1000);
  }
}


void setup()
{
  SerialMon.begin(9600);
  SerialAT.begin(115200);
  lcd.begin();// initialize LCD
  lcd.backlight();// turn on LCD backlight
  lcd.clear();
  lcd.print("Starting...");
  
  //Add CA Certificate
  secure_presentation_layer.setCACert(root_ca);
  
  SerialMon.print("Initializing modem...");
  if (!sim_modem.init()) {
    SerialMon.print(" fail... restarting modem...");
    // Restart takes quite some time
    // Use modem.init() if you don't need the complete restart
    if (!sim_modem.restart()) {
      SerialMon.println(" fail... even after restart");
    } else {
      SerialMon.println(" OK");
    }
  } else {
    SerialMon.println(" OK");
  }

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && sim_modem.getSimStatus() != 3) {
    sim_modem.simUnlock(simPIN);
  }

  // Wait for network availability
  SerialMon.print("Waiting for network...");
  if (sim_modem.waitForNetwork(240000L)) {
    SerialMon.println(" OK");
    // Connect to the GPRS network
    SerialMon.print("Connecting to network...");
    if (sim_modem.isNetworkConnected()) {
      SerialMon.println(" OK");
      // Connect to APN
      SerialMon.print(F("Connecting to APN: "));
      SerialMon.print(apn);
      if (sim_modem.gprsConnect(apn, gprs_user, gprs_pass)) {
        SerialMon.println(" OK");
      } else {
        SerialMon.println(" fail");
      }
    } else {
      SerialMon.println(" fail");
    }
  } else {
    SerialMon.println(" fail");
  }
  
  // More info..
  Serial.println("");
  IPAddress local = sim_modem.localIP();
  Serial.println("Local IP: " + String(local));
  int csq = sim_modem.getSignalQuality();
  Serial.println("Signal quality: " + String(csq));

  //rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
  delay(10);
  Serial.println();
  Serial.println("Starting...");

  
  pinMode(buzzerPin, OUTPUT);
  pinMode(processPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(processPin, LOW);
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
  btn1.onPress(httpRequestHandler)
      .onDoublePress(pressHandler)     // default timeout
      .onPressFor(pressHandler, 1000); // custom timeout for 1 second
  btnManager.addButton(&btn1, 180, 230);
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
  
  Serial.println("Startup is complete");

}


void loop()
{
  //int z;
  //z = analogRead(btnPin);
  //if (z > 100) lcd.print(z);
  //btnManager.printReading(btnPin);
  btnManager.loop();
  
  // setup serial transmission between serial 0 (serial monitor) and serial 2 (sim module)
  while(Serial.available()) {
    delay(1); 
    SerialAT.write(SerialMon.read());
  }
  while(SerialAT.available()) {
    SerialMon.write(SerialAT.read());
  }
  /**/
  static boolean newDataReady = 0;
  const int serialPrintInterval = 100; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      SerialMon.print("Load_cell output val: ");
      SerialMon.println(i);
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

