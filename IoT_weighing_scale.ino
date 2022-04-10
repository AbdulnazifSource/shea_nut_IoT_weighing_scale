#include "IoT_weighing_scale.h"
int httpRequestType;
int httpActionType = 0;
String deviceId, password;
float calibrationValue, knownMass;
String httpStatus, postData, responseData;
String resource = "http://nazifapis.pythonanywhere.com/api/v1/initialize.json"; //arduinojson.org/example.json httpbin.org //anything 

uint8_t dataPin = 23;
uint8_t clockPin = 19;

HX711_ADC LoadCell(dataPin, clockPin);

float price;
unsigned long t = 0;

// sim module
int simRXPin = 16;
int simTXPin = 17;
int simResetPin = 18;

// read buffer
char charBuffer;
String strBuffer;
int maxBufferSize = 30;

// sim module status
boolean isGsmModuleActive = false;

// Set serial for debug console (to the Serial Monitor)
#define SerialMon Serial
// Set serial for AT commands (to the SIM800 module)
#define SerialAT Serial2

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

void reset(BfButton *btn, BfButton::press_pattern_t pattern) {
  int deviceIdAddress = 0, passwordAddress = 100, calibrationValueAddress = 110, knownMassAddress = 120;
  deviceId = "";
  password = "";
  calibrationValue = 832.10;
  knownMass = 1000.0;

  SerialMon.println(" button pressed.");
  SerialMon.println("Clearing EEPROM...");
  for( int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  SerialMon.println("EEPROM Clear Done!");

  EEPROM.put(deviceIdAddress, deviceId);
#if defined(ESP8266)|| defined(ESP32)
  EEPROM.commit();
#endif
  EEPROM.put(passwordAddress, password);
#if defined(ESP8266)|| defined(ESP32)
  EEPROM.commit();
#endif
  EEPROM.put(calibrationValueAddress, calibrationValue);
#if defined(ESP8266)|| defined(ESP32)
  EEPROM.commit();
#endif  
  EEPROM.put(knownMassAddress, knownMass);
#if defined(ESP8266)|| defined(ESP32)
  EEPROM.commit();
#endif
/*#if defined(ESP8266)|| defined(ESP32)
    EEPROM.end();
#endif*/
  getEepromData();
}

void getEepromData() {
  int deviceIdAddress = 0, passwordAddress = 100, calibrationValueAddress = 110, knownMassAddress = 120;
  SerialMon.println("Reading from EEPROM...");
  
  EEPROM.get(deviceIdAddress, deviceId);
  SerialMon.println("device id: " + deviceId);
  EEPROM.get(passwordAddress, password);
  SerialMon.println("password: " + String(password));
  EEPROM.get(calibrationValueAddress,  calibrationValue);
  SerialMon.println("cal val: " + String(calibrationValue));
  EEPROM.get(knownMassAddress, knownMass);
  SerialMon.println("Known mass: " + String(knownMass));
}

void setEepromData() {
  int deviceIdAddress = 0, passwordAddress = 100, calibrationValueAddress = 110, knownMassAddress = 120;
  SerialMon.println("Writing to EEPROM...");
  
  EEPROM.put(deviceIdAddress, deviceId);
  SerialMon.println("device id: " + deviceId);
  EEPROM.put(passwordAddress, password);
  SerialMon.println("password: " + String(password));
  EEPROM.put(calibrationValueAddress,  calibrationValue);
  SerialMon.println("cal val: " + String(calibrationValue));
  EEPROM.put(knownMassAddress, knownMass);
  SerialMon.println("Known mass: " + String(knownMass));
}

boolean ATRun(String command, int waitTime=1000, const char* response="OK\r\n") {
  SerialMon.println("AT" + command);
  SerialAT.println("AT" + command);
  if(waitTime == 0) return true;
  int t = millis();
  String buffer; 
  char ch;
  while(millis() - t < waitTime){
    if (SerialAT.available()){
      ch = SerialAT.read();
      buffer.concat(ch);
      SerialMon.print(ch);
      if (buffer.endsWith(response)) return true;
      if (buffer.endsWith("ERROR\r\n")) return false;
    }
  }
  return false;
}

String ATParser(const char * delimiter, int start, int stop=-1) {
  strBuffer = "";
  String data;
  int countDelimiter = 0;
  while(SerialAT.available()) {
    charBuffer = SerialAT.read();
    strBuffer.concat(charBuffer);
    SerialMon.write(charBuffer);

    if ( countDelimiter == stop || strBuffer.endsWith("\r\nOK\r\n") ) break;
    if ( strBuffer.endsWith("\r\n") && stop == -1 ) break;
    
    if ( strBuffer.endsWith(delimiter) ) {
      countDelimiter++;
    } else if ( countDelimiter == start ) {
      data.concat(charBuffer);
    }
  }
  strBuffer = "";
  return data;
}

void ATLoop() {
  while(SerialAT.available()) {
    charBuffer = SerialAT.read();
    SerialMon.write(charBuffer);
    strBuffer.concat(charBuffer);

    if(strBuffer.endsWith("+CMT:")) {
      parseMessage();
    }
    if (strBuffer.endsWith("+HTTPACTION:")) {
      getHttpResponseStatus();
    }
    if (strBuffer.endsWith("+HTTPREAD:")) {
      getHttpResponseData();
    }
  }
}

void setupGprsConfig(){
  ATRun("+COPS=0,2");
  ATRun("+COPS?", 1000, "+COPS:");
  String networkCode = ATParser("\"", 1, 2);
  ATRun("+COPS=0,0");
  ATRun("+SAPBR=3,1,\"Contype\",\"GPRS\"");
  
  // MTN Bearer configuration
  if (networkCode.equals("62130")) {  
    ATRun("+SAPBR=3,1,\"APN\",\"web.gprs.mtnnigeria.net\"");
    ATRun("+SAPBR=3,1,\"PWD\",\"web\"");
    ATRun("+SAPBR=3,1,\"USER\",\"web\"");
  }
}

void openGprsConn() {
  ATRun("+SAPBR=0,1");
  ATRun("+SAPBR=1,1");
  // open Http connection
  ATRun("+HTTPINIT");
}

void closeGprsConn() {
  // close Http connection
  ATRun("+HTTPTERM");
  //detach device from GPRS
  ATRun("+SAPBR=0,1");
}

void sendHttpRequest() {
  ATRun("+HTTPPARA=\"CID\",1");
  ATRun("+HTTPPARA=\"REDIR\",1");
  ATRun("+HTTPPARA=\"CONTENT\",\"application/json\"");
  ATRun("+HTTPPARA=\"URL\",\"" +  resource + "\"");
  ATRun("+HTTPPARA=\"Device-Id\",\"" +  deviceId + "\"");
  
  if (httpActionType == 1) {
    ATRun("+HTTPDATA=" + String(postData.length()) + ",20000", false);
    SerialAT.print(postData);
  }
  ATRun("+HTTPACTION=" + String(httpActionType), false);
}

void getHttpResponseStatus() {
  httpStatus = ATParser(",", 1, 2);
  httpStatus.trim();
  SerialMon.println("http status: " + httpStatus);
  ATRun("+HTTPREAD", false);
}

void getHttpResponseData() {
  const int COMMAND_REQUEST = 1;
  closeGprsConn();
  responseData = ATParser("\r\n", 1, 2);
  responseData.trim();
  SerialMon.println("Response: " + responseData);
  if (httpRequestType == COMMAND_REQUEST) {
    replyMessage();
  }
  
  strBuffer = "";
}

void sendSms(String number, String message) {
  String command = "AT+CMGS=\"";
  command.concat(number);
  command.concat("\"\r");
  SerialAT.print(command);
  delay(200);
  command = message;
  command.concat((char) 26);
  SerialAT.println(command);
}

void replyMessage() {
  if (httpStatus == "200") {
    const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4);
    StaticJsonDocument<capacity> doc;

    DeserializationError err = deserializeJson(doc, responseData);
    // Parse succeeded?
    if (!err) {
      String reply = doc["reply"].as<String>();
      String phone_number = doc["phone_number"].as<String>();
      String device_id = doc["data"]["device_id"].as<String>();
      if (device_id) {
        deviceId = device_id;
        setEepromData();
      }
      sendSms(phone_number, reply);
      
    } else {
      SerialMon.print("deserializeJson failed");
      SerialMon.println(err.f_str());
    }
  }
}

void processRequest(String contactNumber, String message) {
  const size_t capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;
  const int COMMAND_REQUEST = 1;
  String reply;

  message.toLowerCase();
  if (message.startsWith("wd")) {
    openGprsConn();
    httpRequestType = COMMAND_REQUEST;
    httpActionType = 1;
    resource = "http://nazifapis.pythonanywhere.com/api/v1/smsrequest";

    postData = "";
    doc["message"] = message;
    doc["phone_number"] = contactNumber;
    serializeJson(doc, postData);
    SerialMon.println("post Data: " + postData);
    sendHttpRequest();
  }
}

void parseMessage() {
  String contactNumber  = ATParser("\"", 1, 2);
  String message  = ATParser("\r\n", 1, 2);
  
  message.trim();

  SerialMon.println("number: " + contactNumber);
  SerialMon.println("message: " + message);
  processRequest(contactNumber, message);
}

boolean initializeGsmModem() {
  SerialMon.println("Initializing modem...");
  int timer = millis();
  while(millis() - timer < 5000){
    if (ATRun("")) {
      SerialMon.println("Modem initialzation successful");
      return true;
    }
  }
  return false;
}

boolean checkNetworkReg() {
  ATRun("+CREG?", 2000, "+CREG:");
  String regStatus  = ATParser(",", 1);
  regStatus.trim();
  if (regStatus.equals("1") || regStatus.equals("5")) return true;
  return false;
}

boolean waitNetworkReg() {
  SerialMon.println("Waiting for network...");
  int timer = millis();
  
  while((millis() - timer < 12000)) {
    if (checkNetworkReg()) {
      SerialMon.println("Network available");
      return true;
    } else {
      SerialMon.println(" fail");
    }
  }
  return false;
}

String checkSignalQuality() {
    ATRun("+CSQ", 1000, "+CSQ:");
    String signalQ = ATParser(",", 0, 1);
    signalQ.trim();
    signalQ = (signalQ.toDouble() / 31 * 100) + String("%");
    SerialMon.println("");
    SerialMon.println("Signal quality: " + signalQ);
    return signalQ;
}

void httpRequestHandler(BfButton *btn, BfButton::press_pattern_t pattern) {
  SerialMon.print(btn->getID());
  if (pattern == BfButton::SINGLE_PRESS) {
    SerialMon.println(" button pressed.");
    SerialMon.println("");
    SerialMon.println("Making GET request");
    
    openGprsConn();
    sendHttpRequest();
  }
}

void tare(BfButton *btn, BfButton::press_pattern_t pattern) {
  SerialMon.print(btn->getID());
  if ( BfButton::SINGLE_PRESS == pattern) {
    SerialMon.println("Button pressed.");
    //LoadCell.tareNoDelay();
    LoadCell.tare();
    // check if last tare operation is complete:
    if (LoadCell.getTareStatus() == true) {
      SerialMon.println("Tare complete");
    }
  }
}

void calibration(BfButton *btn, BfButton::press_pattern_t pattern) {
  int calibrationValueAddress = 110;
  LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
  calibrationValue = LoadCell.getNewCalibration(knownMass); //get the new calibration value
  EEPROM.put(calibrationValueAddress, calibrationValue);
#if defined(ESP8266)|| defined(ESP32)
  EEPROM.commit();
#endif
  SerialMon.println("Calibration complete");
  SerialMon.println("new calibration value: " + String(calibrationValue));
}

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern) {
  SerialMon.print(btn->getID());
  switch (pattern)
  {
    case BfButton::SINGLE_PRESS:
    getEepromData();
    break;
  }
}

void setup()
{
  SerialMon.begin(9600);
  SerialAT.begin(115200);
#if defined(ESP8266)|| defined(ESP32)
  EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
#endif  
  getEepromData();
  
  lcd.begin();// initialize LCD
  lcd.backlight();// turn on LCD backlight
  lcd.clear();
  lcd.print("Starting...");


  isGsmModuleActive = initializeGsmModem();
  
  if (isGsmModuleActive) {
    
    if( waitNetworkReg() ) {
      
      checkSignalQuality();
      
      // sms settings
      ATRun("+CMGF=1"); // change sms message format to text mode
      ATRun("+CNMI=2,2,0,0,0"); // send incoming sms directly to serial buffer

      setupGprsConfig();
    }
    else {
      SerialMon.print("Network registration failed");
    }
  }
  
  //rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
  SerialMon.println();
  SerialMon.println("Starting...");

  
  pinMode(buzzerPin, OUTPUT);
  pinMode(processPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(processPin, LOW);
  
  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  
  // uncomment this if you want to set the calibration value in the sketch

  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom
  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
  
  //integrate event handlers to buttons and add buttons to event manager
  btn1.onPress(httpRequestHandler); // custom timeout for 1 second
  btnManager.addButton(&btn1, 180, 250);
  btn2.onPress(pressHandler); // custom timeout for 1 second
  btnManager.addButton(&btn2, 490, 550);
  btn3.onPressFor(reset, 5000); // custom timeout for 1 second
  btnManager.addButton(&btn3, 750, 850);
  //.onPress(httpRequestHandler)
  //.onDoublePress(pressHandler) 
  btn4.onPress(tare)    // default timeout
      .onPressFor(calibration, 5000); // custom timeout for 1 second
  btnManager.addButton(&btn4, 900, 1100);
  btnManager.begin();

  SerialMon.println("Startup is complete");
}

void loop()
{
  //int z;
  //z = analogRead(btnPin);
  //if (z > 100) lcd.print(z);
  btnManager.printReading(btnPin);
  btnManager.loop();

  if(isGsmModuleActive) {
    // setup serial transmission between serial 0 (serial monitor) and serial 2 (sim module)
    while(SerialMon.available()) {
      delay(1); 
      SerialAT.write(SerialMon.read());
    }
    ATLoop();
  }
    
  /**/
  static boolean newDataReady = 0;
  const int serialPrintInterval = 230; //increase value to slow down serial print activity

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
      lcd.print("N");
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