#include "IoT_weighing_scale.h"

// Set serial for debug console (to the Serial Monitor)
#define SerialMon Serial

// Set serial for AT commands (to the SIM800 module)
#ifndef __AVR_ATmega328P__
#define SerialAT Serial2 // rx  16, tx  17

// or Software Serial on Uno, Nano
#else
#include <SoftwareSerial.h>
SoftwareSerial SerialAT(2, 3);  // RX, TX
#endif

// Taskhandle for runing loop2
TaskHandle_t Task1;

int httpRequestType = 0;
int httpActionType = 0;
int noticeStart;
const int noticeDuration = 2500;
boolean showNotification = false;
String deviceId;
float calibrationValue, knownMass;
String httpStatus, postData, responseData;
String resource = "http://nazifapis.pythonanywhere.com/api/v1/initialize"; //arduinojson.org/example.json httpbin.org //anything 
String contactUrl = "http://nazifapis.pythonanywhere.com/api/v1/contact/?limit=5";

struct Rate {
  double quantity;
  double price;
  char unit[11];
  char currency[10];
  boolean active_rate;
};
Rate rateData;

struct Quantity {
  double available_quantity;
  char unit[11];
};
Quantity quantityData;

struct Contact {
  int count;
  int current;
  int total_sent;
  int total_results;
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

#define REQUEST_QUEUE_SIZE 5
RequestQueue requestQueue[REQUEST_QUEUE_SIZE];
int nextRequest = 0;

void (*runRequest)();

uint8_t dataPin = 23; // 4
uint8_t clockPin = 19; // 5

HX711_ADC LoadCell(dataPin, clockPin);
float sensorData;
float price;
unsigned long t = 0;

// read buffer
char charBuffer;
String strBuffer;
int maxBufferSize = 30;

// sim module status
boolean isGsmModuleActive = false;

// buzzer
int buzzerPin = 32; //  6

// processing indicator LED
int processPin = 2; //  13
sllib led(processPin);

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// reset button
int resetPin = En;

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

// push button power supply
int btnPower = 33; // 7

// set button manager and initialize buttons
int btnPin = 34; // A0
int numberOfbuttons = 6;
BfButtonManager btnManager(btnPin, numberOfbuttons);
BfButton btn1(BfButton::ANALOG_BUTTON_ARRAY, 0);
BfButton btn2(BfButton::ANALOG_BUTTON_ARRAY, 1);
BfButton btn3(BfButton::ANALOG_BUTTON_ARRAY, 2);
BfButton btn4(BfButton::ANALOG_BUTTON_ARRAY, 3);
BfButton btn5(BfButton::ANALOG_BUTTON_ARRAY, 4);
BfButton btn6(BfButton::ANALOG_BUTTON_ARRAY, 5);

void loop2(void * parameter) {
  for(;;){
//    if(isGsmModuleActive) {
//      // setup serial transmission between serial 0 (serial monitor) and serial 2 (sim module)
//      while(SerialMon.available()) {
//        delay(1);
//        SerialAT.write(SerialMon.read());
//      }
//      ATLoop();
//      // && httpRequestType == 0
//      if (SerialAT.available() == 0 && httpRequestType == 0){
//        (*runRequest)();
//        runRequest = processRequest;
//      }
//    }
    vTaskDelay(10);
  }
}

void reset() {
  int deviceIdAddress = 0, calibrationValueAddress = 100, knownMassAddress = 110;
  deviceId = "";
  calibrationValue = 832.10;
  knownMass = 1000.0;

  SerialMon.println(F(" button pressed."));
  SerialMon.println(F("Clearing EEPROM..."));
  for( int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  SerialMon.println(F("EEPROM Clear Done!"));
  setEepromData();
  getEepromData();
  notify(F("Device Reset"), F("Complete"), 1);
}

void setEepromData() {
  //const int STR_LENGTH = deviceId.length() + 1;
  char device_id[90];
  int deviceIdAddress = 0, calibrationValueAddress = 100, knownMassAddress = 110;
  SerialMon.println(F("Writing to EEPROM..."));
  deviceId.toCharArray(device_id, sizeof(device_id));
  EEPROM.put(deviceIdAddress, device_id);

  EEPROM.put(calibrationValueAddress, calibrationValue);

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
  char device_id[90];
  int deviceIdAddress = 0, calibrationValueAddress = 100, knownMassAddress = 110;
  SerialMon.println(F("Reading from EEPROM..."));
  
  EEPROM.get(deviceIdAddress, device_id);
  deviceId = device_id;
  SerialMon.println("device id: " + deviceId);
  EEPROM.get(calibrationValueAddress,  calibrationValue);
  SerialMon.println("cal val: " + String(calibrationValue));
  EEPROM.get(knownMassAddress, knownMass);
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
      if (buffer.endsWith(F("ERROR\r\n"))) return false;
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

    if ( countDelimiter == stop || strBuffer.endsWith(F("\r\nOK\r\n")) ) break;
    if ( strBuffer.endsWith(F("\r\n")) && stop == -1 ) break;
    
    if ( strBuffer.endsWith(delimiter) ) countDelimiter++;
    else if ( countDelimiter >= start ) data.concat(charBuffer);
  }
  strBuffer = "";
  return data;
}

void callbackOnFinished(){
#if defined(ESP32)
  ledcDetachPin(buzzerPin);
// or Software Serial on Uno, Nano
#else
  noTone(buzzerPin);
#endif
}

void notify(String firstRow, String secondRow, int beepType) {
  lcd.clear();
  int freeColumns = lcdColumns - firstRow.length();
  int index = freeColumns ? freeColumns / 2 : 0;
  // set the cursor to align text to the center in first row
  lcd.setCursor(index, 0);
  lcd.print(firstRow);
  
  if (secondRow.length() > 0) {
    int freeColumns = lcdColumns - secondRow.length();
    int index = freeColumns ? freeColumns / 2 : 0;
  // set the cursor to align text to the center in the second row
    lcd.setCursor(index, 1);
    lcd.print(secondRow);
  }
  noticeStart = millis();
  if (beepType == 1) EasyBuzzer.beep(1000, 2, callbackOnFinished);
}

void notify(String firstRow) {
  notify(firstRow, "", 0);
}

void notify(String firstRow, int beepType) {
  notify(firstRow, "", beepType);
}

void notify(String firstRow, String secondRow) {
  notify(firstRow, secondRow, 0);
}

void setupGprsConfig(){
  ATRun(F("+COPS=0,2"));
  ATRun("+COPS?", 1000, "+COPS:");
  String carrierCodes = ATParser("\"", 1, 2);
  ATRun("+COPS=0,0");
  ATRun("+SAPBR=3,1,\"Contype\",\"GPRS\"");
  
  // MTN Nigeria GPRS configuration
  if (carrierCodes.equals(F("62130"))) {  
    ATRun("+SAPBR=3,1,\"APN\",\"web.gprs.mtnnigeria.net\"");
    ATRun("+SAPBR=3,1,\"PWD\",\"web\"");
    ATRun("+SAPBR=3,1,\"USER\",\"web\"");
  }
  // GLO Nigeria GPRS configuration
  if (carrierCodes.equals(F("62150"))) {  
    ATRun("+SAPBR=3,1,\"APN\",\"gloflat\"");
    ATRun("+SAPBR=3,1,\"PWD\",\"Flat\"");
    ATRun("+SAPBR=3,1,\"USER\",\"Flat\"");
  }
  // Airtel Nigeria GPRS configuration
  if (carrierCodes.equals(F("62120"))) {  
    ATRun("+SAPBR=3,1,\"APN\",\"internet.ng.airtel.com\"");
    ATRun("+SAPBR=3,1,\"PWD\",\"internet\"");
    ATRun("+SAPBR=3,1,\"USER\",\"internet\"");
  }
  // 9mobile Nigeria GPRS configuration
  if (carrierCodes.equals(F("62160"))) {  
    ATRun("+SAPBR=3,1,\"APN\",\"9mobile\"");
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
  EasyBuzzer.stopBeep();
  const int GET_REQUEST = 0;
  const int POST_REQUEST = 1;
  const int DELETE_REQUEST = 3;
  boolean cmdStatus = false;
  
  openGprsConn();
  // set up request header
  ATRun("+HTTPPARA=\"CID\",1");
  ATRun("+HTTPPARA=\"REDIR\",1");
  ATRun("+HTTPPARA=\"CONTENT\",\"application/json\"");
  ATRun("+HTTPPARA=\"URL\",\"" +  resource + "\"");
  ATRun("+HTTPPARA=\"USERDATA\",\"Device-Id: " + deviceId + "\"");
  
  // send request
  led.setBlinkSingle(300);
  if (httpActionType == POST_REQUEST) {
    ATRun("+HTTPDATA=" + String(postData.length()) + ",10000", 3000, "DOWNLOAD\r\n");
    ATRun(postData, 3000, "OK\r\n", true);
    cmdStatus = ATRun("+HTTPACTION=1");
  } else if (httpActionType == GET_REQUEST){
    cmdStatus = ATRun("+HTTPACTION=0");
  } else if (httpActionType == DELETE_REQUEST) {
    cmdStatus = ATRun("+HTTPACTION=3");
  }
  if (!cmdStatus){
    httpRequestType = 0;  // set request state to none when http request command fails
    closeGprsConn();
    led.setOffSingle();
    notify("Request Failed", 1);
  }
}

void getHttpResponseStatus() {
  const int CLEAR_QUANTITY_REQUEST = 7;
  String dataLength, status;

  status = ATParser("\r\n", 0); // example returns 0,401,0\r\n
  status.trim(); // example returns 0,401,0
  int firstDelimiter = status.indexOf(','); // example returns 1
  int lastDelimiter = status.lastIndexOf(','); // example returns 5
  httpStatus = status.substring(firstDelimiter + 1, lastDelimiter); // example returns 401
  dataLength = status.substring(lastDelimiter + 1); // example returns 0
  SerialMon.println("datalength: " + dataLength);
  SerialMon.println("http status: " + httpStatus);
  
  if (httpRequestType == CLEAR_QUANTITY_REQUEST && httpStatus.equals(F("204"))) {
    // handle response to clear quantity records from database 
    char unit[] = "GRAMME";
    quantityData.available_quantity = 0.00;
    strlcpy(quantityData.unit, unit, 24);
    httpRequestType = 0;
    closeGprsConn();
    led.setOffSingle();
    notify("Records cleared", "Successfully", 1);
  } else if (dataLength.toInt() > 0 && !dataLength.equals(F("500"))){
    // read response data
    ATRun("+HTTPREAD", false); 
  } else {
    httpRequestType = 0;
    closeGprsConn();
    led.setOffSingle();
    notify("Request Failed", 1);
  }
}

void getHttpResponseData() {
  const int SEND_SMS_REQUEST = 1;
  const int STATUS_REQUEST = 3;
  const int QUANTITY_REQUEST = 4;
  const int CONTACT_REQUEST = 5;

  responseData = ATParser("\r\n", 1, 2);
  responseData.trim();
  SerialMon.println("Response: " + responseData);
  closeGprsConn();
  led.setOffSingle();
  if (httpRequestType == SEND_SMS_REQUEST) handleMessageReply();
  else if (httpRequestType == STATUS_REQUEST) storeStatus();
  else if (httpRequestType == QUANTITY_REQUEST) storeQuantity();
  else if (httpRequestType == CONTACT_REQUEST) storeContact();

  httpRequestType = 0;
  strBuffer = "";
}

void sendSms(String message, String number) {
  String command = "AT+CMGS=\"";
  command.concat(number);
  command.concat(F("\"\r"));
  SerialAT.print(command);
  delay(200);
  command = message;
  command.concat((char) 26);
  SerialAT.print(command);
}

void addRequestToQueue(String message, String phoneNumber, int action) {
  int freeSlot = -1;
  // find free slot
  for (int i = 0; i < REQUEST_QUEUE_SIZE; i++) {
    if ( requestQueue[i].action == 0) {
      freeSlot = i;
      break;
    }
  }
  SerialMon.println("free slotfound: " + String(freeSlot));
  if (freeSlot != -1) {
    requestQueue[freeSlot].action = action;
    requestQueue[freeSlot].message = message;
    requestQueue[freeSlot].phoneNumber = phoneNumber;
  }
}

void handleMessageReply() {
  String device_id, message, phoneNumber;
  const int REPLY_SMS_REQUEST = 2;
  const size_t capacity = 400;
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, responseData);
  // Parse succeeded?
  if (!err) {
    device_id = doc["data"]["device_id"] | "";
    if (device_id.length() > 0) {
      deviceId = device_id;
      setEepromData();
    }
    if (doc["login_success"]) {
      notify("Login", "Successful", 1);
    }
    if (doc["registration_success"]) {
      notify("Registration", "Successful", 1);
    }
    message = doc["reply"].as<String>();
    phoneNumber = doc["phone_number"].as<String>();
    addRequestToQueue(message, phoneNumber, REPLY_SMS_REQUEST);
  } else {
    SerialMon.print(F("deserializeJson failed "));
    SerialMon.println(err.f_str());
  }
}
void clearContacts() {
  contact.count = 0;
  contact.current = 0;
  contact.total_sent = 0;
  contact.total_results = 0;
  contact.next = "";
  contact.message = "";
  contact.previous = "";
  for(int i = 0; i < 5; i++) {
    contact.phone_numbers[i][0] = '\0';
  }
}

void clearRequest(int request) {
  requestQueue[request].action = 0;
  requestQueue[request].message = "";
  requestQueue[request].phoneNumber = "";
}
void pointToNextRequest(){
  // increment nextRequest (index) and reset index to 0 if it exceeds the total queue size
  if (++nextRequest == REQUEST_QUEUE_SIZE) nextRequest = 0;
}

void cancelRequest() {
  const int CLIENT_UPDATE_REQUEST = 6;
  //httpRequestType = 0;  // set request state to none when http request command fails
  //led.setOffSingle();
  //closeGprsConn();
  showNotification = false;
  
  if(requestQueue[nextRequest].action == CLIENT_UPDATE_REQUEST) {
    clearContacts();
    clearRequest(nextRequest);
    pointToNextRequest();
    notify("Operation", "Canceled", 1);
  }
}

void processRequest() {
  const int NO_REQUEST = 0;
  const int SEND_SMS_REQUEST = 1;
  const int REPLY_SMS_REQUEST = 2;
  const int CLIENT_UPDATE_REQUEST = 6;
  const int GET = 0;
  int count = 0;
  
  // find request in message queue
  while(count < REQUEST_QUEUE_SIZE && requestQueue[nextRequest].action == NO_REQUEST){
    count++;
    pointToNextRequest();
  }
  
  if (requestQueue[nextRequest].action == SEND_SMS_REQUEST) {
    const size_t capacity = JSON_OBJECT_SIZE(2);
    StaticJsonDocument<capacity> doc;
    
    httpRequestType = SEND_SMS_REQUEST;
    httpActionType = 1;
    resource = "http://nazifapis.pythonanywhere.com/api/v1/smsrequest/";
    postData = "";
    doc["message"] = requestQueue[nextRequest].message.c_str();
    doc["phone_number"] = requestQueue[nextRequest].phoneNumber.c_str();
    serializeJson(doc, postData); // serialize message post
    SerialMon.println("post Data: " + postData);
    notify("Processing", "client request");
    sendHttpRequest();
    clearRequest(nextRequest);
    pointToNextRequest();
  }
  else if(requestQueue[nextRequest].action == REPLY_SMS_REQUEST) {
    SerialMon.println("reply request found: " + String(nextRequest));
    SerialMon.println("message: " + requestQueue[nextRequest].message);
    SerialMon.println("phone number: " + requestQueue[nextRequest].phoneNumber);
    if (requestQueue[nextRequest].message && requestQueue[nextRequest].phoneNumber) {
      notify("Sending response", "to client");
      sendSms(requestQueue[nextRequest].message, requestQueue[nextRequest].phoneNumber);
    }
    clearRequest(nextRequest);
    pointToNextRequest();
  }
  else if(requestQueue[nextRequest].action == CLIENT_UPDATE_REQUEST) {
    char progress[16];
    showNotification = true;
    SerialMon.println("contact update found: " + String(nextRequest));
    snprintf(progress, 16, "%d / %d (%d/%d)", contact.current + 1, contact.total_results, contact.total_sent, contact.count);
    
    notify("Notifying Clients", String(progress));
    sendSms(contact.message, contact.phone_numbers[contact.current]); 
    
    // move to the next number
    if (contact.current < contact.total_results) {
      contact.total_sent++;
      contact.current++;
    } 
    
    if (contact.current == contact.total_results) {
      if(contact.next.length() > 0) {
        contact.current = 0;
        runRequest = updateContact; // fetch the next contact list
      } else {
        showNotification = false;
        notify("Notification", "Complete");
        clearContacts();
        clearRequest(nextRequest);
        pointToNextRequest();
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
  if (message.startsWith(F("wd"))) addRequestToQueue(message, phoneNumber, SEND_SMS_REQUEST);
}

void storeStatus() {
  if (!httpStatus.equals(F("200"))) {
    SerialMon.println("");
    SerialMon.println(F("http request failed"));
    notify("Request failed", 1);
    return;
  }
  char format[30];
  String row2, row1;
  const size_t capacity = 400;
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
    
    snprintf(format, 30, "N%.2f / %.2fg", rateData.price, rateData.quantity);
    row1.concat(format);
    double price = rateData.price;
    if(quantityData.available_quantity > 0) {
      price = (quantityData.available_quantity / rateData.quantity) * rateData.price;
    }
    snprintf(format, 30, "%.2f(N%.2f)", quantityData.available_quantity, price);
    row2.concat(format);
    showNotification = true;
    notify(row1, row2, 1);
  } else {
    SerialMon.print(F("deserializeJson failed"));
    SerialMon.println(err.f_str());
    notify("Failed to update", "data", 1);
  }
}

void updateStatus() {
  const int STATUS_REQUEST = 3;
  const int GET = 0;

  httpRequestType = STATUS_REQUEST;
  httpActionType = GET;
  resource = "http://nazifapis.pythonanywhere.com/api/v1/status/";
  notify("Loading status...");
  sendHttpRequest();
}

void storeQuantity() {
  if (!httpStatus.equals(F("200"))) {
    SerialMon.println("");
    SerialMon.println(F("http request failed"));
    notify("Request failed", 1);
    return;
  }
  const size_t capacity = 100;
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, responseData);
  // Parse succeeded?
  if (!err) {
    quantityData.available_quantity = doc["available_quantity"].as<double>();
    strlcpy(quantityData.unit, doc["unit"] | "GRAMME", 24);
    notify("data saved", "successfully", 1);
  } else {
    SerialMon.print(F("deserializeJson failed"));
    SerialMon.println(err.f_str());
    notify("Failed to save", "data", 1);
  }
}

void updateQuantity() {
  const size_t capacity = 100;
  StaticJsonDocument<capacity> doc;
  const int QUANTITY_REQUEST = 4;
  const int POST = 1;

  httpRequestType = QUANTITY_REQUEST;
  httpActionType = POST;
  resource = "http://nazifapis.pythonanywhere.com/api/v1/quantity/";

  postData = "";
  
  doc["available_quantity"] = sensorData;
  doc["unit"] = "GRAMME";
  //doc["available_quantity"] = 0.00;
  //doc["unit"] = "GRAMME";
  
  serializeJson(doc, postData);
  SerialMon.println("post Data: " + postData);
  notify("Saving data...");
  sendHttpRequest();
}

void clearQuantity() {
  
  const int CLEAR_QUANTITY_REQUEST = 7;
  const int DELETE = 3;

  httpRequestType = CLEAR_QUANTITY_REQUEST;
  httpActionType = DELETE;
  resource = "http://nazifapis.pythonanywhere.com/api/v1/quantity/";
  notify("Clearing records");
  sendHttpRequest();
}

void initializeHttp() {
  if (httpActionType == 0) {
    sendHttpRequest();
  }
}
void storeContact(){
  if (!httpStatus.equals(F("200"))) {
    SerialMon.println("");
    SerialMon.println(F("http request failed"));
    notify("Request failed", 1);
    return;
  }
  const int CLIENT_UPDATE_REQUEST = 6;
  const size_t capacity = 400;
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, responseData);
  // Parse succeeded?
  if (!err) {
    contact.current = 0;
    contact.count = doc["count"].as<int>();
    contact.next = doc["next"] | "";
    contact.previous = doc["previous"] | "";
    contact.total_results = doc["results"]["total_results"].as<int>();
    contact.message = doc["results"]["reply"] | "";
    for(int i = 0; i < contact.total_results; i++) {
      strlcpy(contact.phone_numbers[i], doc["results"]["phone_numbers"][i] | "", 15);
    }
    if (contact.total_results > 0 && contact.message.length() > 0) {
      addRequestToQueue("", "", CLIENT_UPDATE_REQUEST);
      notify("contacts Loaded", "successfully", 1);
    }
    
  } else {
    SerialMon.print(F("deserializeJson failed"));
    SerialMon.println(err.f_str());
    notify("Failed to load", "contacts", 1);
  }
}

void updateContact() {
  const int CONTACT_REQUEST = 5;
  const int GET = 0;

  httpRequestType = CONTACT_REQUEST;
  httpActionType = GET;

  resource = contactUrl;
  if(contact.next.length() > 0) resource = contact.next;
  notify("Loading contacts");
  sendHttpRequest();
}

boolean initializeGsmModem() {
  SerialMon.println(F("Initializing modem..."));
  int timer = millis();
  while(millis() - timer < 5000){
    if (ATRun("")) {
      SerialMon.println(F("Modem initialzation successful"));
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
  SerialMon.println(F("Waiting for network..."));
  int timer = millis();
  
  while((millis() - timer <= 12000)) {
    if (checkNetworkReg()) {
      SerialMon.println(F("Network available"));
      return true;
    } else {
      SerialMon.println(F(" fail"));
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
    SerialMon.println(F("Tare complete"));
    notify("Tare complete", 1);
  }
}

void calibration() {
  int calibrationValueAddress = 100;
  LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
  calibrationValue = LoadCell.getNewCalibration(knownMass); //get the new calibration value
  EEPROM.put(calibrationValueAddress, calibrationValue);
#if defined(ESP8266)|| defined(ESP32)
  EEPROM.commit();
#endif
  SerialMon.println(F("Calibration complete"));
  SerialMon.println("new calibration value: " + String(calibrationValue));
  notify("Calibration", "Complete", 1);
}

void pressHandler (BfButton *btn, BfButton::press_pattern_t pattern) {
  /**/

  int id = btn->getID();
  SerialMon.print(String(F("button ")) + id);
  // beep only for the first 4 buttons
  if (id < 4) {
    // check if GSM module is active
    if ( !isGsmModuleActive) {
      notify("Network Not", "Available", 1);
      return;
    }
    // check if device is logged in
    if(deviceId.length() == 0){
      notify("Please Login", "To Continue", 1);
      return;
    }

    EasyBuzzer.singleBeep(
      1000,	// Frequency in hertz(HZ).
      16,	// Duration of the beep in milliseconds(ms).
      callbackOnFinished
    );
  }
  //btnManager.printReading(btnPin);

  if (id == 0) {
    if (pattern == BfButton::SINGLE_PRESS) {
      // display status
      SerialMon.println(F(" pressed."));
      //getEepromData();
      runRequest = updateStatus;
    }
  } else if(id == 1) {
     //save data to database  
    if (pattern == BfButton::SINGLE_PRESS) {
      SerialMon.println(F(" pressed."));
      runRequest = updateQuantity;
    } 
  } else if(id == 2) {
    // send sms 
    if (pattern == BfButton::SINGLE_PRESS) {
      SerialMon.println(F(" pressed."));
      runRequest = updateContact;
    } 
  } else if(id == 3) {
    // clear database records
    if (pattern == BfButton::SINGLE_PRESS) {
      SerialMon.println(F(" pressed."));
      runRequest = clearQuantity;
    } 
  } else if(id == 4) {
    if (pattern == BfButton::SINGLE_PRESS) {
      // tare 
      SerialMon.println(F(" pressed."));
      tare();
    } else if (pattern == BfButton::LONG_PRESS) {
      // calibrate 
      SerialMon.println(F(" long pressed."));
      calibration();
    }
  } else if(id == 5) {
    
    if (pattern == BfButton::SINGLE_PRESS) {
      //cancel operation
      runRequest = cancelRequest;
      SerialMon.println(F(" pressed."));
    } else if (pattern == BfButton::LONG_PRESS) {
       // reset
      SerialMon.println(F(" long pressed."));
      reset();
    }
  }
}

void ATLoop() {
  while(SerialAT.available()) {
    charBuffer = SerialAT.read();
    SerialMon.write(charBuffer);
    strBuffer.concat(charBuffer);

    if(strBuffer.endsWith(F("+CMT:"))) {
      parseMessage();
    }
    if (strBuffer.endsWith(F("+HTTPACTION:"))) {
      getHttpResponseStatus();
    }
    if (strBuffer.endsWith(F("+HTTPREAD:"))) {
      getHttpResponseData();
    }
  }
}

void setup()
{
  SerialMon.begin(9600);
  SerialAT.begin(115200);
  EasyBuzzer.setPin(buzzerPin);
#if defined(ESP8266)|| defined(ESP32)
  EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
#endif
  getEepromData();
  
  lcd.begin();// initialize LCD
  lcd.backlight();// turn on LCD backlight
  lcd.clear();
  lcd.print(F("Starting..."));
  pinMode(btnPower, OUTPUT);
  digitalWrite(btnPower, HIGH);
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
      SerialMon.println(F("Network registration failed"));
    }
    ATRun("+CREG=1");
  }
  
  SerialMon.println();
  SerialMon.println(F("Starting..."));
  
  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  
  // uncomment this if you want to set the calibration value in the sketch

  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom
  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
  
  //integrate event handlers to buttons and add buttons to event manager
  btn1.onPress(pressHandler) // display status 
      .onPressFor(pressHandler, 2000); // test http request
  btnManager.addButton(&btn1, 200, 350);
  btn2.onPress(pressHandler); //save data to database
  btnManager.addButton(&btn2, 500, 650);
  btn3.onPress(pressHandler); // send sms
  btnManager.addButton(&btn3, 750, 800);
  btn4.onPress(pressHandler);    // clear database records
  btnManager.addButton(&btn4, 980, 1050);
  btn5.onPress(pressHandler)    // tare
      .onPressFor(pressHandler, 5000); // calibrate
  btnManager.addButton(&btn5, 1180, 1280);
  btn6.onPress(pressHandler) //cancel operation
      .onPressFor(pressHandler, 5000); // reset
  btnManager.addButton(&btn6, 1340, 1450);
      
  //.onPressFor(pressHandler, 5000)// custom timeout for 1 second
  //.onDoublePress(pressHandler) 
  btnManager.begin();

  // initial request queue items action to none
  for (int i = 0; i < REQUEST_QUEUE_SIZE; i++) {
    requestQueue[i].action = 0;
  }
  // default request processing function pointer to process sms request
  runRequest = processRequest;

  // setup task for processor core 0
  xTaskCreatePinnedToCore(
    loop2, // task function to run loop
    "loop2", // task name
    10000, // stack size
    NULL, // task parameter
    1, // priority
    &Task1, // taskhandler
    0 // processor core to run loop2 (0 or 1)
  );
  delay(500);
  SerialMon.println(F("Startup is complete"));
}

void loop()
{
  //btnManager.printReading(btnPin);
  btnManager.loop();
  led.update();
  EasyBuzzer.update();
  
  /**/
  static boolean newDataReady = 0;
  const int serialPrintInterval = 230; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;
  
  if ((millis() - noticeStart > noticeDuration) && (showNotification == false) && newDataReady) {
    if (millis() > t + serialPrintInterval) {
      sensorData = LoadCell.getData();
      SerialMon.println(LoadCell.getData());
      SerialMon.print(F("Load_cell output val: "));
      SerialMon.println(sensorData);
      lcd.clear();
      
      price = 0.0;
      if(rateData.quantity && rateData.price) {
        price = (sensorData / rateData.quantity) * rateData.price;
      }
      
      //set display
      if (price) {
        lcd.setCursor(0, 0);
        lcd.print("N");
        if(price < 0)
          lcd.print("0");
        else
          lcd.print(price, 1);
      }
      
      lcd.setCursor(0, 1);
      if(sensorData > 0);
        lcd.setCursor(1, 1);
      lcd.print(sensorData, 1);
      lcd.print("g");

      newDataReady = 0;
      t = millis();
    }
  }
}
