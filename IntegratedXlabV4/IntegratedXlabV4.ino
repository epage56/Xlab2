#include <WiFi.h>       // standard library
#include <WebServer.h>  // standard library
#include "SuperMon.h"   // .h file that stores your html page code
#include <FS.h>
#include <SD.h>
#include <SPI.h>    //SD card
#include <HX711.h>  //Load cell
#include <Wire.h>   //LCD
#include <LiquidCrystal_I2C.h>


#define USE_INTRANET
#define LOCAL_SSID "DukeVisitor"
#define LOCAL_PASS NULL
#define AP_SSID "TestWebSite"
#define AP_PASS "023456789"

// start your defines for pins for sensors, outputs etc.
#define PIN_OUTPUT 26 // connected to nothing but an example of a digital write from the web page
#define PIN_FAN 27    // pin 27 and is a PWM signal to control a fan speed
#define PIN_LED 2     //On board LED
#define PIN_A0 34     // some analog input sensor
#define PIN_A1 35     // some analog input sensor

// variables to store measure data and sensor states
int BitsA0 = 0, BitsA1 = 0;
float VoltsA0 = 0, VoltsA1 = 0;
int FanSpeed = 0;
bool LED0 = false, SomeOutput = false;
uint32_t SensorUpdate = 0;
int FanRPM = 0;
char XML[2048];
char buf[32];
IPAddress Actual_IP;
IPAddress PageIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress ip;

// gotta create a server
WebServer server(80);

//OTHER THINGS FROM INTEGRATION

//sd start 
/*
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       -
 *    D3       SS
 *    CMD      MOSI
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      SCK
 *    VSS      GND
 *    D0       MISO
 *    D1       -
 */


int i = 0;

//sd end

// load cell
#define calibration_factor -100448.4 //FOR LOAD CELL
#define LOADCELL_DAT_PIN  4 //These 2 for load cell
#define LOADCELL_CLK_PIN  15 //These 2 for load cell

// Pin connected to the digital output of the photobreak sensor

const int photobreakPin = 34;
const int photbreakPin2 = 35;

int rpm = 0;
int rpm2 = 0;
unsigned long millisBefore;
unsigned long millisBefore2;
volatile int interruptCount; 
volatile int interruptCount2; 

unsigned long interrupt;
unsigned long interrupt2;

String Weightstr;
String rpmstr;
String rpmstr2;

File myFile;
HX711 scale;

LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD address to 0x27 for a 16 chars and 2 line display

const int relayPin = 35; // Define the pin for the relay switch
const int differenceThreshold = 100; // Set the difference threshold value
const int sustainTime = 3000; // Set the sustain time in milliseconds (3 seconds)

unsigned long startTime = 0; // Variable to store the start time of the difference
bool relayState = false; // Variable to store the state of the relay

int rpm = 0; // Placeholder for rpm value
int rpm2 = 0; // Placeholder for rpm2 value





//SETUP STARTS HERE -------------------------------------

void setup() {

  // standard stuff here
  Serial.begin(9600);

  pinMode(relayPin, OUTPUT); // Set relay pin as output

  if(!SD.begin()){
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    listDir(SD, "/", 0);
    createDir(SD, "/mydir");
    listDir(SD, "/", 0);
    removeDir(SD, "/mydir");
    listDir(SD, "/", 2);
    writeFile(SD, "/hello.txt", "Hello there~313414123 ");
    appendFile(SD, "/hello.txt", "World! Hold on\n");
    readFile(SD, "/hello.txt");
    deleteFile(SD, "/foo.txt");
    renameFile(SD, "/hello.txt", "/foo.txt");
    readFile(SD, "/foo.txt");

    writeFile(SD, "/data.txt", "Index, Time, Load, RPM\n");

    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

    delay(1000);
    
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // LCD CODE START
  lcd.init();                       // Initialize the LCD
  lcd.backlight();                  // Turn on the backlight
  lcd.clear();                      // Clear the LCD screen
  // LCD CODE END
  
  // LOAD CELL SETUP BEGINS HERE
  Serial.println("Finding Scale:");
  scale.begin(LOADCELL_DAT_PIN, LOADCELL_CLK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); //zeros the loadcell
  
  //LOAD CELL SETUP ENDS HERE

  //ENCODER SETUP STARTS HERE

  // Set the photobreak pin as input
  pinMode(photobreakPin, INPUT);
  // Attach an interrupt to the photobreak pin
  attachInterrupt(digitalPinToInterrupt(photobreakPin), photobreakInterrupt, FALLING);

  //ENCODER SETUP ENDS HERE
 
  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  // turn off led
  LED0 = false;
  digitalWrite(PIN_LED, LED0);

  // configure LED PWM functionalitites
  ledcSetup(0, 10000, 8);
  ledcAttachPin(PIN_FAN, 0);
  ledcWrite(0, FanSpeed);

  // if your web page or XML are large, you may not get a call back from the web page
  // and the ESP will think something has locked up and reboot the ESP
  // not sure I like this feature, actually I kinda hate it
  // disable watch dog timer 0
  // disableCore0WDT();

  // maybe disable watch dog timer 1 if needed
    disableCore1WDT();

  // just an update to progress
  Serial.println("starting server");

  // if you have this #define USE_INTRANET,  you will connect to your home intranet, again makes debugging easier
#ifdef USE_INTRANET
  WiFi.begin(LOCAL_SSID, LOCAL_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  Actual_IP = WiFi.localIP();
#endif

  // if you don't have #define USE_INTRANET, here's where you will creat and access point
  // an intranet with no internet connection. But Clients can connect to your intranet and see
  // the web page you are about to serve up
#ifndef USE_INTRANET
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);
  WiFi.softAPConfig(PageIP, gateway, subnet);
  delay(100);
  Actual_IP = WiFi.softAPIP();
  Serial.print("IP address: "); Serial.println(Actual_IP);
#endif

  printWifiStatus();
  
  // these calls will handle data coming back from your web page
  // this one is a page request, upon ESP getting / string the web page will be sent
  server.on("/", SendWebsite);

  // upon esp getting /XML string, ESP will build and send the XML, this is how we refresh
  // just parts of the web page
  server.on("/xml", SendXML);

  // upon ESP getting /UPDATE_SLIDER string, ESP will execute the UpdateSlider function
  // same notion for the following .on calls
  // add as many as you need to process incoming strings from your web page
  // as you can imagine you will need to code some javascript in your web page to send such strings
  // this process will be documented in the SuperMon.h web page code
  server.on("/UPDATE_SLIDER", UpdateSlider);
  server.on("/BUTTON_0", ProcessButton_0);
  server.on("/BUTTON_1", ProcessButton_1);

  // finally begin the server
  server.begin();

}

void loop() {

  //LOAD CELL CHECK IN LOOP
  Serial.print("Reading: ");
  float Weight = scale.get_units();
  if (Weight <= 0.0) {
      Weight = - Weight;
  }
  BitsA1 = Weight;
  Serial.print(Weight, 1);
  Serial.print(" lbs");
  Serial.println();
  //END LOAD CELL CHECK LOOP


  calculateRpm()
  calculateRpm2()
  checkDifference()
  
  lcd.setCursor(0,0);
  lcd.print("RPM");
  lcd.print(rpm);
  lcd.setCursor(0,1);
  lcd.print("Load");
  lcd.print(Weight);
  
  //DO NOT DELETE - SENSOR EXAMPLE
  
  //if ((millis() - SensorUpdate) >= 50) {
    //Serial.println("Reading Sensors");
    //SensorUpdate = millis();
    //BitsA0 = analogRead(PIN_A0);
    //BitsA1 = analogRead(PIN_A1);

    // standard converion to go from 12 bit resolution reads to volts on an ESP
    //VoltsA0 = BitsA0 * 3.3 / 4096;
    //VoltsA1 = BitsA1 * 3.3 / 4096;

  //}
  
  i++;
  
  unsigned long currentMillis = millis(); // Get current time in milliseconds

  // Convert milliseconds to hours, minutes, seconds, and milliseconds
  unsigned long hours = currentMillis / 3600000;
  unsigned long minutes = (currentMillis % 3600000) / 60000;
  unsigned long seconds = ((currentMillis % 3600000) % 60000) / 1000;
  unsigned long milliseconds = currentMillis % 1000;

  // Create a string for the timestamp
  String timestamp = String(hours) + ":" + String(minutes) + ":" + String(seconds) + "." + String(milliseconds);

  // Convert integer i to a string
  String index = String(i);
  Weightstr = String(Weight);
  rpmstr = String(rpm);

  // Combine the index and timestamp with headings
  String entry = index + ", " + timestamp + ", " + Weightstr + ", " + rpmstr + ", " + "\n";

  // Append the entry to the file
  appendFile(SD, "/data.txt", entry.c_str());

  delay(100);


  // no matter what you must call this handleClient repeatidly--otherwise the web page
  // will not get instructions to do something
  server.handleClient();

}


//ENCODER FUNCITONS:

void calculateRpm() {
  if (millis() - millisBefore > 1000) {
    rpm = interruptCount * 60 / 20; 
    BitsA0 = rpm;
    interruptCount = 0; 
    //printer();
    millisBefore = millis();
  }
}

void calculateRpm2() {
  if (millis() - millisBefore2 > 1000) {
    rpm2 = interruptCount2 * 60 / 20; 
    BitsA0 = rpm2;
    interruptCount2 = 0; 
    //printer();
    millisBefore2 = millis();
  }
}

void checkDifference() {

  int difference = abs(rpm - rpm2);

  // Check if the difference is greater than the threshold
  if (difference > differenceThreshold) {
    digitalWrite(relayPin, HIGH); // Activate relay (stops the motor)
  } else {
    digitalWrite(relayPin, LOW); // motor on
  } 
}


//START SD FUNCTIONS

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}



//END SD FUNCTIONS



//START ENCODER FUNCTIONS

void printer() {
  // Serial.println("interrruptCount:");
  Serial.println(rpm);
}

// Interrupt service routine to count interruptions
void photobreakInterrupt() {
  // Increment the interruption count
  interruptCount++;
}

//END ENCODER FUNCTIONS


// function managed by an .on method to handle slider actions on the web page
// this example will get the passed string called VALUE and conver to a pwm value
// and control the fan speed
void UpdateSlider() {

  // many I hate strings, but wifi lib uses them...
  String t_state = server.arg("VALUE");

  // conver the string sent from the web page to an int
  FanSpeed = t_state.toInt();
  Serial.print("UpdateSlider"); Serial.println(FanSpeed);
  // now set the PWM duty cycle
  ledcWrite(0, FanSpeed);


  // YOU MUST SEND SOMETHING BACK TO THE WEB PAGE--BASICALLY TO KEEP IT LIVE

  // option 1: send no information back, but at least keep the page live
  // just send nothing back
  // server.send(200, "text/plain", ""); //Send web page

  // option 2: send something back immediately, maybe a pass/fail indication, maybe a measured value
  // here is how you send data back immediately and NOT through the general XML page update code
  // my simple example guesses at fan speed--ideally measure it and send back real data
  // i avoid strings at all caost, hence all the code to start with "" in the buffer and build a
  // simple piece of data
  FanRPM = map(FanSpeed, 0, 255, 0, 2400);

  strcpy(buf, "");
  sprintf(buf, "%d", FanRPM);
  sprintf(buf, buf);

  // now send it back
  server.send(200, "text/plain", buf); //Send web page

}


// now process button_0 press from the web site. Typical applications are the used on the web client can
// turn on / off a light, a fan, disable something etc

void ProcessButton_0() {

  //

  LED0 = !LED0;
  digitalWrite(PIN_LED, LED0);
  Serial.print("Button 0 "); Serial.println(LED0);
  // regardless if you want to send stuff back to client or not
  // you must have the send line--as it keeps the page running
  // if you don't want feedback from the MCU--or let the XML manage
  // sending feeback

  // option 1 -- keep page live but dont send any thing
  // here i don't need to send and immediate status, any status
  // like the illumination status will be send in the main XML page update
  // code
  server.send(200, "text/plain", ""); //Send web page

  // option 2 -- keep page live AND send a status
  // if you want to send feed back immediataly
  // note you must have reading code in the java script
  /*
    if (LED0) {
    server.send(200, "text/plain", "1"); //Send web page
    }
    else {
    server.send(200, "text/plain", "0"); //Send web page
    }
  */

}

// same notion for processing button_1
void ProcessButton_1() {

  // just a simple way to toggle a LED on/off. Much better ways to do this
  Serial.println("Button 1 press");
  SomeOutput = !SomeOutput;

  digitalWrite(PIN_OUTPUT, SomeOutput);
  Serial.print("Button 1 "); Serial.println(LED0);
  // regardless if you want to send stuff back to client or not
  // you must have the send line--as it keeps the page running
  // if you don't want feedback from the MCU--or send all data via XML use this method
  // sending feeback
  server.send(200, "text/plain", ""); //Send web page

  // if you want to send feed back immediataly
  // note you must have proper code in the java script to read this data stream
  /*
    if (some_process) {
    server.send(200, "text/plain", "SUCCESS"); //Send web page
    }
    else {
    server.send(200, "text/plain", "FAIL"); //Send web page
    }
  */
}


// code to send the main web page
// PAGE_MAIN is a large char defined in SuperMon.h
void SendWebsite() {

  Serial.println("sending web page");
  // you may have to play with this value, big pages need more porcessing time, and hence
  // a longer timeout that 200 ms
  server.send(200, "text/html", PAGE_MAIN);

}

// code to send the main web page
// I avoid string data types at all cost hence all the char mainipulation code
void SendXML() {

  // Serial.println("sending xml");

  strcpy(XML, "<?xml version = '1.0'?>\n<Data>\n");

  // send bitsA0
  sprintf(buf, "<B0>%d</B0>\n", BitsA0);
  strcat(XML, buf);
  // send Volts0
  sprintf(buf, "<V0>%d.%d</V0>\n", (int) (VoltsA0), abs((int) (VoltsA0 * 10)  - ((int) (VoltsA0) * 10)));
  strcat(XML, buf);

  // send bits1
  sprintf(buf, "<B1>%d</B1>\n", BitsA1);
  strcat(XML, buf);
  // send Volts1
  sprintf(buf, "<V1>%d.%d</V1>\n", (int) (VoltsA1), abs((int) (VoltsA1 * 10)  - ((int) (VoltsA1) * 10)));
  strcat(XML, buf);

  // show led0 status
  if (LED0) {
    strcat(XML, "<LED>1</LED>\n");
  }
  else {
    strcat(XML, "<LED>0</LED>\n");
  }

  if (SomeOutput) {
    strcat(XML, "<SWITCH>1</SWITCH>\n");
  }
  else {
    strcat(XML, "<SWITCH>0</SWITCH>\n");
  }

  strcat(XML, "</Data>\n");
  // wanna see what the XML code looks like?
  // actually print it to the serial monitor and use some text editor to get the size
  // then pad and adjust char XML[2048]; above
  Serial.println(XML);

  // you may have to play with this value, big pages need more porcessing time, and hence
  // a longer timeout that 200 ms
  server.send(200, "text/xml", XML);


}

// I think I got this code from the wifi example
void printWifiStatus() {

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("Open http://");
  Serial.println(ip);
}

// end of code
