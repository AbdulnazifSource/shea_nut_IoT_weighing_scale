#include "IoT_weighing_scale.h"

// Set serial for debug console (to the Serial Monitor)
#define SerialMon Serial

// Set serial for AT commands (to the SIM800 module)
#ifndef __AVR_ATmega328P__
#define SerialAT Serial2

// or Software Serial on Uno, Nano
#else
#include <SoftwareSerial.h>
SoftwareSerial SerialAT(2, 3);  // RX, TX
#endif

int httpRequestType = 0;
int httpActionType = 0;
String deviceId, password;
float calibrationValue, knownMass;
String httpStatus, postData, responseData;
String resource = "http://nazifapis.pythonanywhere.com/api/v1/initialize"; //arduinojson.org/example.json httpbin.org //anything 
 
struct Rate {
  double quantity;
  double price;
  char unit[11];
  char currency[25];
  boolean active_rate;
};
Rate rateData;

struct Quantity {
  double available_quantity;
  char unit[24];
};
Quantity quantityData;

struct Contact {
  int count;
  int current;
  String next;
  String previous;
  String message;
  char phone_numbers[5][15];
};
Contact contact;

struct RequestQueue {
  int action;
  String message;
  String phoneNumber;
};

RequestQueue requestQueue[5];
int nextRequest = 0;

void (*runRequest)();

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
int numberOfbuttons = 6;
BfButtonManager btnManager(btnPin, numberOfbuttons);
BfButton btn1(BfButton::ANALOG_BUTTON_ARRAY, 0);
BfButton btn2(BfButton::ANALOG_BUTTON_ARRAY, 1);
BfButton btn3(BfButton::ANALOG_BUTTON_ARRAY, 2);
BfButton btn4(BfButton::ANALOG_BUTTON_ARRAY, 3);
BfButton btn5(BfButton::ANALOG_BUTTON_ARRAY, 4);
BfButton btn6(BfButton::ANALOG_BUTTON_ARRAY, 5);

void reset() {
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

boolean ATRun(String command, int waitTime=2000, const char* response="OK\r\n", boolean data=false) {
  if (data) {
    SerialMon.println(command);
    SerialAT.println(command);
  } else {
    SerialMon.println("AT" + command);
    SerialAT.println("AT" + command);
  }
      
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



void setupGprsConfig(){
  ATRun("+COPS=0,2");
  ATRun("+COPS?", 1000, "+COPS:");
  String networkCode = ATParser("\"", 1, 2);
  ATRun("+COPS=0,0");
  ATRun("+SAPBR=3,1,\"Contype\",\"GPRS\"");
  
  // MTN GPRS configuration
  if (networkCode.equals("62130")) {  
    ATRun("+SAPBR=3,1,\"APN\",\"web.gprs.mtnnigeria.net\"");
    ATRun("+SAPBR=3,1,\"PWD\",\"web\"");
    ATRun("+SAPBR=3,1,\"USER\",\"web\"");
  }
}

void openGprsConn() {
  ATRun("+SAPBR=0,1");
  ATRun("+SAPBR=1,1", 5000);
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
  const int GET_REQUEST = 0;
  const int POST_REQUEST = 1;

  ATRun("+HTTPPARA=\"CID\",1");
  ATRun("+HTTPPARA=\"REDIR\",1");
  ATRun("+HTTPPARA=\"CONTENT\",\"application/json\"");
  ATRun("+HTTPPARA=\"URL\",\"" +  resource + "\"");
  ATRun("+HTTPPARA=\"USERDATA\",\"Device-Id: " + deviceId + "\"");
  
  if (httpActionType == GET_REQUEST) {
    ATRun("+HTTPACTION=0", false);
  } else if (httpActionType == POST_REQUEST) {
    ATRun("+HTTPDATA=" + String(postData.length()) + ",10000", 3000, "DOWNLOAD\r\n");
    ATRun(postData, 3000, "OK\r\n", true);
    ATRun("+HTTPACTION=1");
  }
}

void getHttpResponseStatus() {
  httpStatus = ATParser(",", 1, 2);
  httpStatus.trim();
  SerialMon.println("");
  SerialMon.println("http status: " + httpStatus);
  ATRun("+HTTPREAD", false);
}

void getHttpResponseData() {
  const int SEND_SMS_REQUEST = 1;
  const int STATUS_REQUEST = 3;
  const int QUANTITY_REQUEST = 4;
  const int CONTACT_REQUEST = 5;
  //String dataLength = ATParser("\r\n", 0);
  //dataLength.trim();
  //int dataLength.toInt();
  responseData = ATParser("\r\n", 1, 2);
  responseData.trim();
  SerialMon.println("Response: " + responseData);
  closeGprsConn();
  if (httpRequestType == SEND_SMS_REQUEST) {
    handleMessageReply();
  } else if (httpRequestType == STATUS_REQUEST) {
    storeStatus();
  } else if (httpRequestType == QUANTITY_REQUEST) {
    storeQuantity();
  } else if (httpRequestType == CONTACT_REQUEST) {
    storeContact();
  }
  httpRequestType = 0;
  strBuffer = "";
}

void sendSms(const String& message, const String& number) {
  String command = "AT+CMGS=\"";
  command.concat(number);
  command.concat("\"\r");
  SerialAT.print(command);
  delay(200);
  command = message;
  command.concat((char) 26);
  SerialAT.println(command);
}

void addRequestToQueue(const String& message, const String& phoneNumber, int action) {
  int freeSlot = -1;
  // find free slot
  for (int i = 0; i < 5; i++) {
    if ( requestQueue[i].action == 0) {
      freeSlot = i;
    }
  }
  if (freeSlot != -1) {
    requestQueue[freeSlot].action = action;
    requestQueue[freeSlot].message = message;
    requestQueue[freeSlot].phoneNumber = phoneNumber;
  }
}

void handleMessageReply() {
  String device_id, message, phoneNumber;
  const int REPLY_SMS_REQUEST = 2;
  const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, responseData);
  // Parse succeeded?
  if (!err) {
    device_id = doc["data"]["device_id"].as<String>();
    if (device_id.length() > 0) {
      deviceId = device_id;
      setEepromData();
    }
    message = doc["reply"].as<String>();
    phoneNumber = doc["phone_number"].as<String>();
    addRequestToQueue(message, phoneNumber, REPLY_SMS_REQUEST);
  } else {
    SerialMon.print("deserializeJson failed");
    SerialMon.println(err.f_str());
  }
}
void clearRequest(int request) {
  requestQueue[request].action = 0;
  requestQueue[request].message = "";
  requestQueue[request].phoneNumber = "";
}
void processRequest() {
  const int NO_REQUEST = 0;
  const int SEND_SMS_REQUEST = 1;
  const int REPLY_SMS_REQUEST = 2;
  const int CLIENT_UPDATE_REQUEST = 6;
  const int GET = 0;
  if (requestQueue[nextRequest].action == SEND_SMS_REQUEST) {
    const size_t capacity = JSON_OBJECT_SIZE(2);
    StaticJsonDocument<capacity> doc;
    
    openGprsConn();
    httpRequestType = SEND_SMS_REQUEST;
    httpActionType = 1;
    resource = "http://nazifapis.pythonanywhere.com/api/v1/smsrequest/";
    postData = "";
    doc["message"] = requestQueue[nextRequest].message.c_str();
    doc["phone_number"] = requestQueue[nextRequest].phoneNumber.c_str();
    serializeJson(doc, postData); // serialize message post
    SerialMon.println("post Data: " + postData);
    sendHttpRequest();
    clearRequest(nextRequest);
    if (++nextRequest > 5) {
      nextRequest = 0;
    }
  }
  else if(requestQueue[nextRequest].action == REPLY_SMS_REQUEST) {
    if (requestQueue[nextRequest].message && requestQueue[nextRequest].phoneNumber) {
      sendSms(requestQueue[nextRequest].message, requestQueue[nextRequest].phoneNumber);
    }
    clearRequest(nextRequest);
    if (++nextRequest > 5) {
      nextRequest = 0;
    }
  }
  else if(requestQueue[nextRequest].action == CLIENT_UPDATE_REQUEST) {
    sendSms(contact.message, contact.phone_numbers[contact.current]); 
    contact.current++; // move to the next number
    if (contact.current == contact.count) {
      if(contact.next.length() != 0){
        runRequest = updateContact; // fetch the next contact list
      } else {
        clearRequest(nextRequest);
        if (++nextRequest > 5) {
          nextRequest = 0;
        }
      }
    }
  }
}

void parseMessage() {
  const int SEND_SMS_REQUEST = 1;
  String phoneNumber  = ATParser("\"", 1, 2);
  String message  = ATParser("\r\n", 1, 2);
  
  message.trim();
  message.toLowerCase();
  SerialMon.println("");
  SerialMon.println("number: " + phoneNumber);
  SerialMon.println("message: " + message);
  // serialize json
  if (message.startsWith("wd")) {
    addRequestToQueue(message, phoneNumber, SEND_SMS_REQUEST);
  }
}

void storeStatus() {
  if (!httpStatus.equals("200")) {
    SerialMon.println("");
    SerialMon.println("http request failed");
    return;
  }
  const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5);
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, responseData);
  // Parse succeeded?
  if (!err) {
    rateData.quantity = doc["rate"]["quantity"].as<double>();
    rateData.price = doc["rate"]["price"].as<double>();
    strlcpy(rateData.unit, doc["rate"]["unit"] | "GRAMME", 11);
    strlcpy(rateData.currency, doc["rate"]["currency"] | "NAIRA", 25);
    rateData.active_rate = doc["rate"]["active_rate"].as<boolean>();
    quantityData.available_quantity = doc["quantity"]["available_quantity"].as<double>();
    strlcpy(quantityData.unit, doc["quantity"]["unit"] | "GRAMME", 24);
  } else {
    SerialMon.print("deserializeJson failed");
    SerialMon.println(err.f_str());
  }
}

void updateStatus() {
  const int STATUS_REQUEST = 3;
  const int GET = 0;

  openGprsConn();
  httpRequestType = STATUS_REQUEST;
  httpActionType = GET;
  resource = "http://nazifapis.pythonanywhere.com/api/v1/status";

  sendHttpRequest();
}

void storeQuantity() {
  if (!httpStatus.equals("200")) {
    SerialMon.println("");
    SerialMon.println("http request failed");
    return;
  }
  const size_t capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, responseData);
  // Parse succeeded?
  if (!err) {
    quantityData.available_quantity = doc["available_quantity"].as<double>();
    strlcpy(quantityData.unit, doc["unit"] | "GRAMME", 24);
  } else {
    SerialMon.print("deserializeJson failed");
    SerialMon.println(err.f_str());
  }
}

void updateQuantity() {
  const size_t capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;
  const int QUANTITY_REQUEST = 4;
  const int POST = 1;

  openGprsConn();
  httpRequestType = QUANTITY_REQUEST;
  httpActionType = POST;
  resource = "http://nazifapis.pythonanywhere.com/api/v1/quantity/";

  postData = "";
  
  doc["available_quantity"] = quantityData.available_quantity;
  doc["unit"] = quantityData.unit;
  //doc["available_quantity"] = 0.00;
  //doc["unit"] = "GRAMME";
  
  serializeJson(doc, postData);
  SerialMon.println("post Data: " + postData);
  sendHttpRequest();
}

void initializeHttp() {
  if (httpActionType == 0) {
    openGprsConn();
    sendHttpRequest();
  }
}
void storeContact(){
  if (!httpStatus.equals("200")) {
    SerialMon.println("");
    SerialMon.println("http request failed");
    return;
  }
  const int CLIENT_UPDATE_REQUEST = 6;
  const size_t capacity = JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(5);
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, responseData);
  // Parse succeeded?
  if (!err) {
    contact.current = 0;
    contact.count = doc["count"].as<int>();
    contact.next = doc["next"].as<String>();
    contact.previous = doc["previous"].as<String>();
    contact.message = doc["results"]["reply"].as<String>();
    for(int i = 0; i < contact.count; i++) {
      strlcpy(contact.phone_numbers[i], doc["results"]["phone_numbers"][i] | "", 15);
    }
    addRequestToQueue("empty", "empty", CLIENT_UPDATE_REQUEST);
  } else {
    SerialMon.print("deserializeJson failed");
    SerialMon.println(err.f_str());
  }
}

void updateContact() {
  const int CONTACT_REQUEST = 5;
  const int GET = 0;

  openGprsConn();
  httpRequestType = CONTACT_REQUEST;
  httpActionType = GET;
  resource = contact.next;

  sendHttpRequest();
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
  
  while((millis() - timer <= 12000)) {
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

void tare() {
  //LoadCell.tareNoDelay();
  LoadCell.tare();
  // check if last tare operation is complete:
  if (LoadCell.getTareStatus() == true) {
    SerialMon.println("Tare complete");
  }
}

void calibration() {
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

void pressHandler (BfButton *btn, BfButton::press_pattern_t pattern) {
  /**/
  int id = btn->getID();
  SerialMon.print(String("button ") + id);

  if (id == 0) {
    if (pattern == BfButton::SINGLE_PRESS) {
      // display status
      SerialMon.println(" pressed.");
      getEepromData();
      runRequest = initializeHttp;
    } else if (pattern == BfButton::DOUBLE_PRESS) {
      SerialMon.println(" double pressed.");
      runRequest = updateStatus;
    } else if (pattern == BfButton::LONG_PRESS) {
      SerialMon.println(" long pressed.");
    }
  } else if(id == 1) {
     //save data to database  
    if (pattern == BfButton::SINGLE_PRESS) {
      SerialMon.println(" pressed.");
      runRequest = updateQuantity;
    } else if (pattern == BfButton::DOUBLE_PRESS) {
      SerialMon.println(" double pressed.");
    } else if (pattern == BfButton::LONG_PRESS) {
      SerialMon.println(" long pressed.");
    }
  } else if(id == 2) {
    // send sms 
    if (pattern == BfButton::SINGLE_PRESS) {
      SerialMon.println(" pressed.");
      runRequest = updateContact;
    } else if (pattern == BfButton::DOUBLE_PRESS) {
      SerialMon.println(" double pressed.");
    } else if (pattern == BfButton::LONG_PRESS) {
      SerialMon.println(" long pressed.");
    }
  } else if(id == 3) {
    // clear database records
    if (pattern == BfButton::SINGLE_PRESS) {
      SerialMon.println(" pressed.");
      quantityData.available_quantity = 0.00;
      strlcpy(quantityData.unit, "GRAMME", 24);
      runRequest = updateQuantity;
    } else if (pattern == BfButton::DOUBLE_PRESS) {
      SerialMon.println(" double pressed.");
    } else if (pattern == BfButton::LONG_PRESS) {
      SerialMon.println(" long pressed.");
    }
  } else if(id == 4) {
     
    if (pattern == BfButton::SINGLE_PRESS) {
      // tare 
      SerialMon.println(" pressed.");
      tare();
    } else if (pattern == BfButton::DOUBLE_PRESS) {
      SerialMon.println(" double pressed.");
    } else if (pattern == BfButton::LONG_PRESS) {
      // calibrate 
      SerialMon.println(" long pressed.");
      calibration();
    }
  } else if(id == 5) {
    
    if (pattern == BfButton::SINGLE_PRESS) {
      //cancel operation
      SerialMon.println(" pressed.");
    } else if (pattern == BfButton::DOUBLE_PRESS) {
      SerialMon.println(" double pressed."); 
    } else if (pattern == BfButton::LONG_PRESS) {
       // reset
      SerialMon.println(" long pressed.");
      reset();
    }
  }
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
    if (strBuffer.endsWith("+CREG:")) {
      String regStatus  = ATParser("\r\n", 0);
      regStatus.trim();
      if (regStatus.equals("1") || regStatus.equals("5")) {
        checkSignalQuality();
        // sms settings
        ATRun("+CMGF=1"); // change sms message format to text mode
        ATRun("+CNMI=2,2,0,0,0"); // send incoming sms directly to serial buffer
        setupGprsConfig();
      }
    }
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
    ATRun("+CREG=1");
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
  btn1.onPress(pressHandler); // display status 
  btnManager.addButton(&btn1, 180, 250);
  btn2.onPress(pressHandler); //save data to database
  btnManager.addButton(&btn2, 490, 550);
  btn3.onPress(pressHandler); // send sms
  btnManager.addButton(&btn3, 750, 850);
  btn4.onPress(pressHandler);    // clear database records
  btnManager.addButton(&btn4, 900, 1100);
  btn5.onPress(pressHandler)    // tare
      .onPressFor(pressHandler, 5000); // calibrate
  btnManager.addButton(&btn5, 1250, 1500);
  btn6.onPress(pressHandler) //cancel operation
      .onPressFor(pressHandler, 5000); // reset
  btnManager.addButton(&btn6, 1750, 1850);
      
  //.onPressFor(pressHandler, 5000)// custom timeout for 1 second
  //.onDoublePress(pressHandler) 
  btnManager.begin();

  // initial request queue items action to none
  for (int i = 0; i < 5; i++) {
    requestQueue[i].action = 0;
  }
  // default request processing function pointer to process sms request
  runRequest = processRequest;

  SerialMon.println("Startup is complete");
}

void loop()
{
  //btnManager.printReading(btnPin);
  btnManager.loop();

  if(isGsmModuleActive) {
    // setup serial transmission between serial 0 (serial monitor) and serial 2 (sim module)
    while(SerialMon.available()) {
      delay(1); 
      SerialAT.write(SerialMon.read());
    }
    ATLoop();
    // && httpRequestType == 0
    if (SerialAT.available() == 0 && httpRequestType == 0){
      (*runRequest)();
      runRequest = processRequest;
    }
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