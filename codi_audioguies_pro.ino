/***************************************************
   CODI AUDIOGUIES SANTA MÒNICA
   Written by Marc Villanueva Mir marcvillanuevamir@gmail.com
   Last edit: 08/07/2023
   Hardware:
  - Arduino Nano
  - DFPlayer Pro
  - RC522
  - x1 Led, x1 pushbutton, x1 toggle switch, x1 9V battery
 ****************************************************/

#include <SPI.h>
#include <MFRC522.h>
#include "Arduino.h"
#include <DFRobot_DF1201S.h>
#include <SoftwareSerial.h>

//these macros can be toggled to turn on/off some capabilities
#define DEBUG true                                      //default: true. change to false in order to prevent serial communication with computer
#define DEBUG_SERIAL if(DEBUG)Serial
#define START_FROM_THE_BEGINNING false                  //default: false. change to true in order to deactivate timestamp function and always start playback from the beginning

//global declarations
DFRobot_DF1201S df;                                     //create and name DFPlayer object "df"
SoftwareSerial dfSerial(6, 7);                          //define RX and TX pins for software serial communication with DFPlayer
boolean compareArray(byte array1[], byte array2[]);
void pcdInitialise();
int timeCalculation();
void getUID();
byte getFilenumber();
void compareTag();
void stopButton();
void blinkLed(int rate);
void fade(byte fadeTime, byte maxVol);
int readArray();
#define RST_PIN  9                                      //define reset pin for RC522
#define SS_PIN  10                                      //define SS pin (SDA) for RC522
MFRC522 mfrc522(SS_PIN, RST_PIN);                       //create and name RC522 object
#define button A3                                       //pin for the stop button
#define powerLed A2                                     //pin for the led
bool ledState;                                          //state of the led
byte filenumber = 0;
byte currentPlay = 0;                                   //variable to store currently playing file number
unsigned long currentMillis;                            //variable to store elapsed time in millis
unsigned long previousMillis;                           //variable to store previously recorded time in millis
byte maxVol = 20;                                       //max volume (0-30)
const int index = 12;                                   //enter the number of files in the memory for indexing the arrays

//storing arrays for RFID tags
byte ActualUID[4];                                      //storing array for every new tag read
byte nextArray[4];                                      //used to compare array after array, as rows in a multidimensional array
byte arrayTag[index][4] = {                             //multidimensional array to store tag UIDs
  {0xFD, 0x6A, 0x49, 0x59},                 //audio 1
  {0x43, 0x34, 0xDB, 0xBD},                 //audio 2
  {0x63, 0x8E, 0x1E, 0xBE},                 //audio 3
  {0x53, 0x0A, 0x30, 0xBE},                 //audio 4
  {0x03, 0xFB, 0x2C, 0xBE},                 //audio 5
  {0x53, 0x72, 0xE5, 0xBD},                 //audio 6
  {0xE6, 0x46, 0xC7, 0x43},                 //audio 7
  {0x73, 0xE3, 0xB9, 0x16},                 //audio 8
  {0x36, 0x05, 0x18, 0x44},                 //audio 9
  {0xF6, 0x74, 0xA0, 0x43},                 //audio 10
  {0x35, 0x48, 0xE1, 0x54},                 //audio 11
  {0x25, 0xBE, 0xE8, 0x54}                  //audio 12
};

//storing array for the files' timelengths
int length1, length2, length3, length4, length5, length6, length7, length8, length9, length10, length11, length12;
int lengths[index] = {length1, length2, length3, length4, length5, length6, length7, length8, length9, length10, length11, length12};

//storing array for the ongoing timestamp calculation
int time1, time2, time3, time4, time5, time6, time7, time8, time9, time10, time11, time12;
int timestamps[index] = {time1, time2, time3, time4, time5, time6, time7, time8, time9, time10, time11, time12};

//storing array for the calculation of cycled time
int set1, set2, set3, set4, set5, set6, set7, set8, set9, set10, set11, set12;
int setTimestamp[index] = {set1, set2, set3, set4, set5, set6, set7, set8, set9, set10, set11, set12};

void setup() {
  pinMode(button, INPUT_PULLUP);
  pinMode(powerLed, OUTPUT);
  digitalWrite(powerLed, HIGH);
  dfSerial.begin(115200);                               //initialise software serial communication with DFPlayer
  DEBUG_SERIAL.begin(115200);                           //initialise hardware serial communication with computer
#if DEBUG == true
  while (!Serial);
#endif
  delay(150);
  digitalWrite(powerLed, LOW);
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println(F("Audioguies Santa Mònica - INITIALIZING"));
  SPI.begin();                                          //initialise SPI bus
  mfrc522.PCD_Init();                                   //initialise RC522
  attachInterrupt(0, pcdInitialise, RISING);            //IRQ pin connected to Arduino pin 2
  delay(10);

  //initialise RC522 RFID reader
  DEBUG_SERIAL.println(F("Initializing RC522 ..."));
  bool result = mfrc522.PCD_PerformSelfTest();          //perform a RC522 self-test
  if (result) {
    mfrc522.PCD_SetAntennaGain(0xFF);                   //set antenna gain for RC522
    delay(50);
    DEBUG_SERIAL.println(F("RC522 online."));
    blinkLed(50);                                       //blink when the RFID reader is online
  } else {
    DEBUG_SERIAL.println(F("RC522 DEFECT or UNKNOWN"));
    while (!result) {
      ledState = digitalRead(powerLed);
      digitalWrite(powerLed, !ledState);
      delay(1000);
    }
  }
  delay(10);

  //----initialise DFPlayer----
  DEBUG_SERIAL.println(F("Initializing DFPlayer Pro ..."));
  if (!df.begin(dfSerial)) {
    DEBUG_SERIAL.println(F("DFPLAYER UNABLE TO BEGIN:"));
    while (!df.begin(dfSerial)) {
      ledState = digitalRead(powerLed);
      digitalWrite(powerLed, !ledState);
      delay(1000);
    }
  }
  df.setVol(0);                                         //set volume to 0 for preparation tasks
  df.switchFunction(df.MUSIC);                          //select mode for playing files
  delay(1000);                                          //delay to avoid the "Music" voice message when initializing the player
  df.setPlayMode(df.SINGLECYCLE);                       //once it starts playing a file, loop it
  df.disableAMP();                                      //disable amp for power saving
  DEBUG_SERIAL.println(F("DFPlayer Pro online."));
  blinkLed(50);                                         //blink when the player is online

  //----Read and adjust information----
  byte filesInMemory = df.getTotalFile();               //read all file counts in memory
  DEBUG_SERIAL.print("Number of files available to play: ");
  DEBUG_SERIAL.println(filesInMemory);                      
  if (filesInMemory > index) {                          //uploading files from iOS might cause hidden files to be copied to the player's memory. If that is the case, display a warning.
    DEBUG_SERIAL.println("Warning! More files in the player than expected. Internal indexing of files might fail. Check the player for hidden files.");
  }
  lengths[index] = readArray();
  df.setVol(maxVol);                                    //set volume
  DEBUG_SERIAL.print("Volume: ");
  DEBUG_SERIAL.println(df.getVol());                    //read current volume
  DEBUG_SERIAL.print(F("Antenna gain: "));
  byte gain = mfrc522.PCD_GetAntennaGain();             //read RC522 antenna's gain
  DEBUG_SERIAL.println(gain);
  if (gain < 112) {                                     //check if antenna gain has been properly set to maximum and else repeat the instruction
    mfrc522.PCD_SetAntennaGain(0xFF);
    DEBUG_SERIAL.print(F("Recalculating antenna gain: "));
    gain = mfrc522.PCD_GetAntennaGain();
    DEBUG_SERIAL.println(gain);
  }
#if START_FROM_THE_BEGINNING == true
  DEBUG_SERIAL.println(F("In this mode, tracks will always start from the beginning"));
#endif
  digitalWrite(powerLed, HIGH);                         //light the power led once the setup is complete
  DEBUG_SERIAL.println(F("Ready."));
}

void loop() {
  currentMillis = millis();
  setTimestamp[index] = timeCalculation();

  if (mfrc522.PICC_IsNewCardPresent()) {                //check if a new card is present
    if (mfrc522.PICC_ReadCardSerial()) {                // select a card
      getUID();
      filenumber = getFilenumber();                     //call the function to establish which filenumber should play
      compareTag();                                     //call the player function to play the file associated with the tag
      mfrc522.PICC_HaltA();                             //stop the reading
      delay(200);
      attachInterrupt(0, pcdInitialise, RISING);        //RC522's IRQ pin connected to Arduino pin 2
    }
  }
  stopButton();
}

void pcdInitialise() {                                  //callback function for interrupt to initialise the RC522
  mfrc522.PCD_Init();
  detachInterrupt(0);
}

int timeCalculation() {
  if (currentMillis - previousMillis >= 1000UL) {
    previousMillis = currentMillis;
    for (byte i = 0; i < index; i++) {
      setTimestamp[i] = timestamps[i] ++ % lengths[i];
    }
    return setTimestamp[index];
  }
}

void getUID() {
  DEBUG_SERIAL.print(F("Card UID:"));                   //send UID via serial
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    DEBUG_SERIAL.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    DEBUG_SERIAL.print(mfrc522.uid.uidByte[i], HEX);
    ActualUID[i] = mfrc522.uid.uidByte[i];
  } DEBUG_SERIAL.print("     ");
}

byte getFilenumber() {
  for (byte i = 0; i < index; i++) {                    //compare UID to recognize which filenumber should activate
    for (byte j = 0; j < 4; j++) {
      nextArray[j] = arrayTag[i][j];
    }
    if (compareArray(ActualUID, nextArray)) {
      return i + 1;
    }
  }
}

void compareTag() {                                     //player function
  blinkLed(75);
  if (currentPlay != filenumber) {
    df.setVol(0);
    df.playFileNum(filenumber);
#if START_FROM_THE_BEGINNING == true
    setTimestamp[filenumber - 1] = 0;
#endif
    df.setPlayTime(setTimestamp[filenumber - 1]);
    currentPlay = filenumber;
    fade(0, maxVol);
#ifdef DEBUG_SERIAL
    char buffer[40];
    sprintf(buffer, "Playing Audio %d from second %d of %d", filenumber, setTimestamp[filenumber - 1], lengths[filenumber - 1]);
    DEBUG_SERIAL.println(buffer);
  } else {
    char buffer[25];
    sprintf(buffer, "Audio %d already playing", filenumber);
    DEBUG_SERIAL.println(buffer);
#endif
  }
}

boolean compareArray(byte array1[], byte array2[]) {    //compare two values and return true if the UID matches the value
  for (byte i = 0; i < 4; i++) {
    if (array1[i] != array2[i])return (false);
  }
  return (true);
}

void stopButton() {                                     //use the pushbutton for stopping playback
  if (digitalRead(button) == LOW) {
    DEBUG_SERIAL.println(F("Button is pressed."));
    df.pause();
    currentPlay = 0;
    delay(10);
  }
}

void blinkLed(int rate) {                               //quickly blink whenever a tag is read
  for (byte i = 0; i < 6; i++) {
    ledState = digitalRead(powerLed);
    digitalWrite(powerLed, !ledState);
    delay(rate);
  }
}

void fade(byte fadeTime, byte maxVol) {                 //fade in audio volume
  for (byte i = 0; i <= maxVol; i++) {                  //increase volume value (0~30)
    df.setVol(i);
    delay(fadeTime);
  }
}

int readArray() {                                       //read the length of the files and stores them in an array
  df.playFileNum(1);
  for (byte i = 0; i < index; i++) {
    lengths[i] = df.getTotalTime();
    delay(10);
    df.next();
  }
  df.pause();
  DEBUG_SERIAL.print(F("Time lengths: "));
  for (byte i = 0; i < index; i++) {
    DEBUG_SERIAL.print(lengths[i]);
    DEBUG_SERIAL.print(" ");
  } DEBUG_SERIAL.println();
  return lengths[index];
}
