#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ThingerWifi.h>

#define USERNAME "username"
#define DEVICE_ID "DevID"
#define DEVICE_CREDENTIAL "credential"
#define _DEBUG_ 1
#define SSID "yourSSID"
#define SSID_PASSWORD "password"

ThingerWifi thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);
//External Connections
unsigned int ledOpen = D5;
unsigned int ledClosed = D3;
unsigned int openPin = D7;
unsigned int closedPin = D6;
unsigned int relayPin = D2;
//Global Variables
unsigned int closeAfterTime = 300000;
unsigned int oldDoorStatus = 0;
unsigned int doorStatus = 99;
unsigned long doorOpenTime = 0;
unsigned long doorOpenedTime = millis();

String openText = " Door is open: ";
String closedText = " Door is closed: ";
String operatingText = " Door is operating: ";

void setup() {

  pinMode(ledOpen, OUTPUT);
  pinMode(ledClosed, OUTPUT);
  pinMode(openPin, INPUT_PULLUP);
  pinMode(closedPin, INPUT_PULLUP);
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(relayPin, OUTPUT);
  thing.add_wifi(SSID, SSID_PASSWORD);
  thing["doorStatus"] >> [](pson & out) {
    out = doorStatus;
  };
  thing.handle();

  Serial.begin(115200);
}

void loop() {
  Serial.println("******* Loop Start *********");

  if (doorStatus == 99) {
    Serial.println("Notify of reboot");
    callEndpoints();
    doorStatus = doorCheck();
  }
  thing.handle();
  //blink heartbeat light to reduce cycles and allow wifi to process
  heartbeatLED();

  //Check status of sensors
  unsigned int myDoorStatus = doorCheck();

  //Call The endpoints to register if door opened or closed
  //Figure out logic of when and what to send to endpoint
  // See State Table.
  //***if oldDoorStatus = (2 or 3) and myDoorStatus = 1 then call endpoint
  //***if old and my doorstatus = 1, then check time and activate if needed
  //Check if door open longer than closeAfterTime
  //***if old = 1 or 2 and myDoorStatus = 3 then call endpoint


  if ((oldDoorStatus == 2 || oldDoorStatus == 3) && myDoorStatus == 1) {//Door has Opened
    doorOpenedTime = 0; //since door just opened, put a zero in the spreadsheet
    callEndpoints();
    doorOpenedTime = millis(); //Now the door is open, remember the time
  }
  else if (oldDoorStatus == 1 && myDoorStatus == 1) {//Door is open
    doorOpenTime = millis() - doorOpenedTime;
    if (doorOpenTime > closeAfterTime) {
      energizeRelay();
    }
  }
  else if ((oldDoorStatus == 1 || oldDoorStatus == 2) && myDoorStatus == 3) {//Door has closed
    callEndpoints();
    doorOpenTime = 0;
  }
  else
    //when door is closed, stuck, or broken

    Serial.println("No Action Door Status");

  oldDoorStatus = myDoorStatus;
}


//******************************************************************************************
// Functions

unsigned doorCheck() {
  //Check the status of the switches
  // Open=1, Closed=3, Operating=2, Problem=9
  Serial.print(digitalRead(openPin));
  Serial.print(digitalRead(closedPin));
  if ((digitalRead(openPin) == 1) && (digitalRead(closedPin) == 1)) {
    //Door is operating
    doorStatus = 2;
    digitalWrite(ledOpen, HIGH);
    digitalWrite(ledClosed, HIGH);
    Serial.println(operatingText);
  }
  else if (digitalRead(openPin) == 0) {
    //Door is open
    doorStatus = 1;
    digitalWrite(ledOpen, HIGH);
    digitalWrite(ledClosed, LOW);
    Serial.println(openText);
    Serial.print("doorOpenTime: ");
    Serial.println(doorOpenTime);
  }
  else if (digitalRead(closedPin) == 0) {
    //Door is closed
    doorStatus = 3;
    //Set the open time = current time
    doorOpenTime = millis();
    digitalWrite(ledOpen, LOW);
    digitalWrite(ledClosed, HIGH);
    Serial.println(closedText);
  }

  else {
    //Both switches failed
    doorStatus = 9;
    Serial.println("Door is busted");
    digitalWrite(ledOpen, LOW);
    digitalWrite(ledClosed, LOW);
  }
  return doorStatus;
}
void gdmInit() {
  //run once on boot
}
void heartbeatLED() {
  //Heartbeat
  //digitalWrite(ledOpen, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(BUILTIN_LED, HIGH);
  delay(1000);              // wait for a second
  //digitalWrite(ledOpen, LOW);    // turn the LED off by making the voltage LOW
  digitalWrite(BUILTIN_LED, LOW);
  delay(5000);              // wait for a while
  return;
}

void callEndpoints() {
  Serial.println("Calling Endpoint now");
  //add a row to the change table
  pson data;
  data["state"] = doorStatus;
  data["GDMtime"] = doorOpenTime;
  thing.call_endpoint("JSON_Testing", data);
  //thing.call_endpoint("GDM2_Endpoint", data);
  //thing.call_endpoint("MakerTest1");
}

void energizeRelay() {
  //close relay to activate door opener
  Serial.println("Relay On");
  digitalWrite(relayPin, HIGH);
  //delay for 1 second to keep relay closed
  delay(1000);
  //notify of autoclose
  unsigned int oldDoorTemp = doorStatus;
  doorStatus = 9;
  callEndpoints();
  doorStatus = oldDoorTemp;
  digitalWrite(relayPin, LOW);
  Serial.println("Relay Off");
  //delay to let door close usually around 13 seconds
  delay(15000);
}
