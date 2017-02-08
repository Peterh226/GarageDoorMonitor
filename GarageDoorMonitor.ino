//#define _DEBUG_
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ThingerWifi.h>

#define USERNAME "peterh226"
#define DEVICE_ID "GDM"
#define DEVICE_CREDENTIAL "#Po&xxxxxxxx"
#define SSID "Bulldog_Guest"
#define SSID_PASSWORD "xxxxxxxxxxxxxx"
// This script is part of a garage door monitoring system.
// please see https://github.com/Peterh226/GarageDoorMonitor for details

ThingerWifi thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);
//External Connections
unsigned int ledOpen = D5;
unsigned int ledClosed = D3;
unsigned int openPin = D7;
unsigned int closedPin = D6;
unsigned int relayPin = D2;
//Global Variables
float closeAfterTime = .083;  //hours
float msCloseAfterTime = 300000;  //milliseconds
unsigned int hourToMillisec = 3600000;
unsigned int oldDoorStatus = 0;
unsigned int doorStatus = 9;
unsigned long doorOpenTime = 0;
unsigned long doorOpenedTime = millis();
unsigned int autoCloseEnable = 1; //set to 0 using API Explorer in Thinger.IO
unsigned int myDoorStatus = 9;

const char* openText = " Door is open";
const char* closedText = " Door is closed";
const char* operatingText = " Door is operating";
const char* autoClosedText = "Door Auto closed";
const char* doorStatusText = "GDM Reboot";

void setup() {
  Serial.begin(115200);
  pinMode(ledOpen, OUTPUT);
  pinMode(ledClosed, OUTPUT);
  pinMode(openPin, INPUT_PULLUP);
  pinMode(closedPin, INPUT_PULLUP);
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(relayPin, OUTPUT);

  thing.add_wifi(SSID, SSID_PASSWORD);

  thing["doorStatus"] >> [](pson & out) {
    out["State"] = myDoorStatus;
    out["DoorText"] = doorStatusText;
    out["doorOpenTime"] = doorOpenTime;
  };

  thing["AutoCloseToggle"] << [](pson & in) {
    autoCloseEnable = in;
    Serial.print("Auto Close Value: ");
    Serial.println(autoCloseEnable);
  };
}

void loop() {
  Serial.println("******* Loop Start *********");
  thing.handle();
  //update autoclose time if new value is provided
  msCloseAfterTime = closeAfterTime * hourToMillisec;
  Serial.print("closeAfter - msClose: ");
  Serial.print(closeAfterTime);
  Serial.print(" - ");
  Serial.println(msCloseAfterTime);

  if (doorStatus == 9) {
    Serial.println("Notify of reboot");
    callEndpoints();
  }

  //blink heartbeat light to reduce cycles and allow wifi to process
  heartbeatLED();

  //Check status of sensors
  myDoorStatus = doorCheck();

  //Call The endpoints to register if door opened or closed
  //Figure out logic of when and what to send to endpoint
  // See State Table.
  //***if oldDoorStatus = (2 or 3) and myDoorStatus = 1 then call endpoint
  //***if old and my doorstatus = 1, then check time and activate if needed
  //Check if door open longer than closeAfterTime
  //***if old = 1 or 2 and myDoorStatus = 3 then call endpoint
  Serial.print("OldStatus - Status: ");
  Serial.print(oldDoorStatus);
  Serial.print(" - ");
  Serial.println(myDoorStatus);
  if ((oldDoorStatus == 2 || oldDoorStatus == 3) && myDoorStatus == 1) {//Door has Opened
    doorOpenedTime = 0; //since door just opened, put a zero in the spreadsheet
    callEndpoints();
    doorOpenedTime = millis(); //Now the door is open, remember the time
  }
  else if (oldDoorStatus == 1 && myDoorStatus == 1) {//Door is open
    doorOpenTime = millis() - doorOpenedTime;
    if ((doorOpenTime > msCloseAfterTime) && (autoCloseEnable == 1)) {
      Serial.println("Auto Energizing Relay");
      Serial.print("Auto Close Value: ");
      Serial.println(autoCloseEnable);
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
    doorStatusText = operatingText;
  }
  else if (digitalRead(openPin) == 0) {
    //Door is open
    doorStatus = 1;
    digitalWrite(ledOpen, HIGH);
    digitalWrite(ledClosed, LOW);
    Serial.println(openText);
    doorStatusText = openText;
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
    doorStatusText = closedText;
  }

  else {
    //Both switches failed
    doorStatus = 8;
    Serial.println("Door is busted");
    digitalWrite(ledOpen, LOW);
    digitalWrite(ledClosed, LOW);
  }
  return doorStatus;
}

void heartbeatLED() {
  //Heartbeat 5second depay
  //digitalWrite(ledOpen, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(BUILTIN_LED, HIGH);
  delay(4000);              // wait for a 4 seconds
  //digitalWrite(ledOpen, LOW);    // turn the LED off by making the voltage LOW
  digitalWrite(BUILTIN_LED, LOW);
  delay(1000);              // LED On for 1 second out of 5
  return;
}

void callEndpoints() {
  Serial.println("Calling Endpoint now");

  //pson data;
  Serial.print(doorStatus);
  Serial.print("---");
  Serial.println(doorOpenTime);
  //add a row to the Google Sheet using IFTTT
  pson txtMsg;
  txtMsg["value1"] = doorStatusText;
  txtMsg["value2"] = doorOpenTime;
  thing.call_endpoint("IFTTT_GDM_Data", txtMsg);

  if (doorStatus == 9) {
    //Notify that system rebooted or auto-closed
    thing.call_endpoint("MakerTest1", txtMsg);
  }
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
  Serial.println("Calling endpoint with auto status");
  doorStatusText = autoClosedText;
  callEndpoints();
  doorStatus = oldDoorTemp;
  digitalWrite(relayPin, LOW);
  Serial.println("Relay Off");
  //delay to let door close usually around 13 seconds
  delay(15000);
}
