//https://api.particle.io/v1/devices/380042001247343339383037/setTemp
//HEADERS:
//Authorization: Bearer 98f018d520a9877ca6939bfb49e0d1d6923f1ce7
//Content-Type: application/json
//BODY:
//{"arg":"28"}

//Next up:
//Publish Variables on any change
//Ability to Start & Stop cooker
//Ability to set total time
/*
 * Project photon
 * Description:
 * Author:
 * Date:
 */

//--------------------------------------------------------------
// Included libraries
//--------------------------------------------------------------

 // This #include statement was automatically added by the Particle IDE.
#include "OneWire.h"
#include "spark-dallas-temperature/spark-dallas-temperature.h"
#include "LiquidCrystal_I2C_Spark.h"
#include <math.h>
#include <string.h>
#include <Encoder.h>

LiquidCrystal_I2C *lcd;

//--------------------------------------------------------------
// Variables
//--------------------------------------------------------------

SYSTEM_THREAD(ENABLED); //enables system functions to happen in a separate thread from the application setup and loop
//this includes connecting to the network and the cloud

//Pin declarations
int ONE_WIRE_BUS = A5;
int relayPin = D4;
int encoderPin1 = D3;
int encoderPin2 = D2;

 // Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
DeviceAddress thermometer1 = { 0x28, 0x94, 0xE2, 0xDF, 0x02, 0x00, 0x00, 0xFE };

Encoder encoder(encoderPin1, encoderPin2);

//Variables
int defaultTemp = 60;
double desiredTemperature;
char *messageTopLine = "Current"; //message on top line of display
char *messageBottomLine = "Target"; //message on bottom line of display

unsigned long lastSensorReading = 0; //time of last sensor reading
unsigned long sensorReadingDelay = 100; //minimum milliseconds between processing successive temperature readings

unsigned long lastRelayCommand = 0; //time of last command to relay
unsigned long relayCommandDelay = 5000; //minimum milliseconds between succesive commands to relay

unsigned long lastScreenRedraw = 0; //time of last screen redraw
unsigned long screenRedrawDelay = 600000; //minimum milliseconds between successive redraws of screen

float temperature = 0; //current temperature

bool connectedOnce = false; //connected to cloud

long encoderNewPosition = -999;
long encoderOldPosition  = -999;

long hour = 3600000; // 3600000 milliseconds in an hour
long minute = 60000; // 60000 milliseconds in a minute
long second =  1000; // 1000 milliseconds in a second

//--------------------------------------------------------------
// Setup
//--------------------------------------------------------------

// setup() runs once, when the device is first turned on.
void setup() {

  // Set pin modes
  pinMode(ONE_WIRE_BUS, INPUT);
  pinMode(relayPin, OUTPUT);

  loadDesiredTemperature();

  //initialize the temperature sensor
  sensors.begin();
  // set the resolution to 10 bit (good enough?)
  sensors.setResolution(thermometer1, 12);

  // initialize the lcd
  initializeDisplay();

  //initialize rotary encoder
  encoderOldPosition = encoder.read();
}

//--------------------------------------------------------------
// Loop
//--------------------------------------------------------------

// loop() runs over and over again, as quickly as it can execute.
void loop() {

  int encoderDirection = readEncoderDirection();
  setDesiredTemperature(desiredTemperature + encoderDirection);

  //code to register cloud functions once the particle is connected
  if (connectedOnce == false) {
    if (Particle.connected()) {
      // Register the setCloudTemp with the Particle cloud, to allow it to be called via the internet
      Particle.function("setTemp", setCloudTemp);
      connectedOnce = true;
    }
  }

  //process a sensor reading if the minimum time has elapsed
  if ( millis() - lastSensorReading >= sensorReadingDelay) {
    float sensorTemperature;
    sensorTemperature = getSensorTemperature();
    if(sensorTemperature > -127){
      temperature = sensorTemperature;
    }

    //record time of last sensor reading
    lastSensorReading = millis();

    //write the current temperature to the screen
    writeScreenCurrentTemperature();
  }

  //set the relay if the minimum time has elapsed
  if (( millis()  - lastRelayCommand >= relayCommandDelay) || (lastRelayCommand == 0)) {
    //if  more than the buffer amount above the desired temperature, switch the relay off
    if ((temperature +0.2) < desiredTemperature) {
      digitalWrite(relayPin, LOW);
    } else {
      digitalWrite(relayPin, HIGH);
    }
    //record time of latest command to relay
    lastRelayCommand  = millis();
  }

  //redraw the screen if the minimum time has elapsed
  if (( millis()  - lastScreenRedraw >= screenRedrawDelay) ||(lastScreenRedraw == 0)){
    //clear the screen and rewrite the various text elements
    lcd->clear();
    writeScreenTopMessage();
    writeScreenBottomMessage();
    writeScreenCurrentTemperature();
    writeScreenDesiredTemperature();
    //record time of latest screen redraw
    lastScreenRedraw = millis();
  }
//end of loop
}

//---------------------------------------------------------------
// Functions
//---------------------------------------------------------------

// Method to set the desired temperature
int setCloudTemp(String temp) {
  setDesiredTemperature(temp.toFloat());
  return desiredTemperature;
}

void setDesiredTemperature(int temperature){
  desiredTemperature = temperature;
  EEPROM.put(10, desiredTemperature);
  writeScreenDesiredTemperature();
}

void loadDesiredTemperature(){
  // Load the desired temp from storage if available, otherwise use the default
  EEPROM.get(10,desiredTemperature);
  if (desiredTemperature == 0xFFFF) {
    // Stored memory is empty, so set it to default temp
    desiredTemperature = defaultTemp;
    // write the desired temp to stored memory for next time
    EEPROM.put(10, defaultTemp);
  }
}

//method to initialize the lcd
void initializeDisplay(){
  //initialize the display
  lcd = new LiquidCrystal_I2C(0x27, 16 , 2);
  lcd->init();
  lcd->backlight();
  lcd->clear();
}

// Method to read temperature
float getSensorTemperature(){
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);
    return tempC;
}

// Method to write to top line of lcd
void writeScreenTopMessage(){
  lcd->setCursor(0,0);
  lcd->print(messageTopLine);
}

//Method to write to bottom line of lcd
void writeScreenBottomMessage(){
  lcd->setCursor(0,1);
  lcd->print(messageBottomLine);
}

// Method to write current temperature to the lcd
void writeScreenCurrentTemperature(){
  lcd->setCursor(11,0);
  lcd->print(temperature);
}

//Method to write the desired temperature to the lcd
void writeScreenDesiredTemperature(){
  lcd->setCursor(11,1);
  lcd->print(desiredTemperature);
}

int readEncoderDirection(){
  encoderNewPosition = encoder.read();
  float returnValue = 0.25 * (encoderNewPosition - encoderOldPosition);
  encoderOldPosition = encoderNewPosition;
  return round(returnValue);
}
//-----------------------------------------------
//Not currently in use
//-----------------------------------------------

//method to return current time as a string
String time(){
  long timeNow = millis();
  //number of days
  int hours = timeNow / hour; //the remainder from days division (in milliseconds) divided by hours, this gives the full hours
  int minutes = ((timeNow ) % hour) / minute ;         //and so on...
  int seconds = (((timeNow ) % hour) % minute) / second;

  // digital clock display of current time
   String f = "";
   f += printDigits(hours);
   f += ":";
   f += printDigits(minutes);
   f += ":";
   f += printDigits(seconds);

   return f;
}

//pad a number with a leading 0 if needed
String printDigits(byte digits){
  // utility function for digital clock display: prints colon and leading 0
  String t = "";

  if(digits < 10) t += '0';
  t += digits;
  return t;
}
