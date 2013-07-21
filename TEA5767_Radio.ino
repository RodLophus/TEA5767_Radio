
/*

Simple radio tuner using Arduino Leonardo and the Philips TEA5767 module


Notes:
-----
- The TEA5767 works with 3.3V. Use an 3.3V powered Arduino (e.g.: Freeduino Leonardo) or an level shifter
- The LCD display must be powered by the 5V reail.  Most LCD modules works fine with 3.3V signal levels, but some does not
- The TEA5767 does not update the signal level indicator on read.  The signal level will only be update on station change
- To modify this code for use with other Arduino boards: just use the PinChangeInt library instead of Arduino´s native 
  attachInterrupt() and it should work fine (not tested).  REMEMBER TO POWER YOUR ARDUINO BY A 3.3V SUPPLY
  (e.g.: http://learn.adafruit.com/arduino-tips-tricks-and-techniques/3-3v-conversion) OR USE LEVEL SHIFTERS,
  OR YOU WILL DAMAGE THE RADIO MODULE! 


Connections:
-----------
- Encoder (with "pushable" shaft switch):
Push button     ---> Arduino pin 0
Encoder pin "A" ---> Arduino pin 1
Encoder pin "B" ---> Arduino pin 4


- LCD:
D7 ---> Arduino pin 8
D6 ---> Arduino pin 9
D5 ---> Arduino pin 10
D4 ---> Arduino pin 11
RS ---> Arduino pin 13
RW ---> GND
E ----> Arduino pin 12
VO ---> 2k2 resistor to GND (contrast)


- TEA5756 module:

Top view:
+-10--9--8--7--6-+
|  +------+  ++  |
|  | TEA  |  ||  |
|  | 5767 |  ||  |
|  +------+  ++  |
+--1--2--3--4--5-+

1 ----> Arduino SDA
2 ----> Arduino SCL
3 ----> GND
5 ----> +3.3V
6 ----> GND
7 ----> Audio out (right channel)
8 ----> Audio out (left channel)
10 ---> Antena

Thank you for your interest.
Have fun!
rodolfo.manin@gmail.com

*/


#include <LiquidCrystal.h>
#include <Wire.h>

// Get TEA5767 library at https://github.com/andykarpov/TEA5767
#include <TEA5767.h>

// Encoder pins
#define ENCODER_SW 0
#define ENCODER_A  1
#define ENCODER_B  4

// Custom characters
#define SCALE_CLEAR   5    // Radio dial scale
#define STEREO_CHAR_S 6    // Stylized "S"
#define STEREO_CHAR_T 7    // Stylized "T"

// Global status flags
#define ST_AUTO    0      // Auto mode (toggled by the push button)
#define ST_STEREO  1      // Radio module detected a stereo pilot
#define ST_GO_UP   2      // Encoder being turned clockwise
#define ST_GO_DOWN 3      // Encoder being turned counterclockwise
#define ST_SEARCH  4      // Radio module is perfoming an automatic search

TEA5767 Radio;
float frequency = 88;
byte status = 0;

LiquidCrystal lcd(13, 12, 11, 10, 9, 8);

/*******************************************\
 *             updateScale()               *
 * Moves the "needle" over the radio scale *
\*******************************************/
void updateScale() {
  int lcdBase = (frequency  - 88) * 4;  // LCD column pixel index (0 <= lcdBase <= (16 * 5))
  if(lcdBase > 79) lcdBase = 79;
  
  int lcdMajor = lcdBase / 5;    // LCD character index (0 <= lcdMajor <= 15)
  int lcdMinor = lcdBase % 5;    // LCD pixel column index within the character (0 <= lcdMinor <= 4)
  
  if(lcdMajor > 0) {
    // The radio dial needle is not at the leftmost position:
    // clean the character on the left (to erase previous neddle position)
    lcd.setCursor(lcdMajor  - 1, 0);
    lcd.write(SCALE_CLEAR);
  } else
    lcd.setCursor(lcdMajor, 0);
  lcd.write(lcdMinor);
  
  if(lcdMajor < 15)
    // Not at rightmost position: clear the character on the right
    lcd.write(SCALE_CLEAR);
}


/******************************\
 *         isrEncoder()       *
 * Catch encoder´s interrupts *
\******************************/
void isrEncoder() {
  delay(50);    // Debouncing (for crappy encoders)
  if(digitalRead(ENCODER_B) == HIGH){
    bitWrite(status, ST_GO_UP, 1);
  } else
    bitWrite(status, ST_GO_DOWN, 1);
}


/*****************************\
 *        isrSwitch()        *
 * Catch switch´s interrupts *
\*****************************/
void isrSwitch() {
  delay(50);    // Debouncing
  if(bitRead(status, ST_AUTO))
    bitWrite(status, ST_AUTO, 0);
  else
    bitWrite(status, ST_AUTO, 1);
}


/*******************\
 * Arduino Setup() *
\*******************/
void setup() {
  int i;
  byte needleChar[8];

  // Stylized "S"
  byte stereoChar1[8] = {
    0b01111,
    0b11000,
    0b11011,
    0b11101,
    0b11110,
    0b11000,
    0b01111,
    0b00000
  };
  lcd.createChar(STEREO_CHAR_S, stereoChar1);

  // Stylized "T"
  byte stereoChar2[8] = {
    0b11110,
    0b00011,
    0b10111,
    0b10111,
    0b10111,
    0b10111,
    0b11110,
    0b00000
  };
  lcd.createChar(STEREO_CHAR_T, stereoChar2);

  // Dial scale background
  byte scaleChar[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00100,
    0b10101,
    0b10101
  };
  lcd.createChar(SCALE_CLEAR, scaleChar);

  // Create custom character to represent all (5) possible needle´s position
  for(int j = 0; j < 5; j++) {
    for(i = 0; i < 8; i++)
      needleChar[i] = scaleChar[i] | (0b10000 >> j);
    lcd.createChar(j, needleChar);
  }

  lcd.begin(16, 2);
  lcd.clear();
  
  // Draw the dial scale´s background
  for(i = 0; i < 16; i++)
    lcd.write(SCALE_CLEAR);
   
  pinMode(ENCODER_SW, INPUT); digitalWrite(ENCODER_SW, HIGH);
  pinMode(ENCODER_A, INPUT);  digitalWrite(ENCODER_A, HIGH);
  pinMode(ENCODER_B, INPUT);  digitalWrite(ENCODER_B, HIGH);
  
  // Arduino Leonardo has interrupts 2 and 3 (for pins 0 and 1).
  // You can use the PinChangeInt to modify this code for other Arduinos
  // (pins 2 and 3 have interrupts on all Arduinos, but they are being used by the TEA5767´s I2C interface)
  attachInterrupt(2, isrSwitch, RISING);
  attachInterrupt(3, isrEncoder, RISING);

  // Initialize the radio module
  Wire.begin();
  Radio.init();
  Radio.set_frequency(frequency);
}


/******************\
 * Arduino Loop() *
\******************/
void loop() {
  unsigned char buf[5];
  int stereo;
  int signalLevel;
  int searchDirection;
  
  // Update the Auto / Manual indicator
  lcd.setCursor(12, 1);
  lcd.write(bitRead(status, ST_AUTO) ? 'A' : 'M');

  if (Radio.read_status(buf) == 1) {
    // Get radio data
    frequency = floor(Radio.frequency_available(buf) / 100000 + .5) / 10;
    stereo = Radio.stereo(buf);
    // 0 <= Radio.signal_level <= 15
    signalLevel = (Radio.signal_level(buf) * 100) / 15;

    // Update the radio dial
    updateScale();
    
    // Signal level indicator
    lcd.setCursor(0, 1);
    lcd.write(183);    // Japanese character that looks like an antenna :)
    if(signalLevel < 100) lcd.write(' ');
    lcd.print(signalLevel);
    lcd.write('%');

    // Frequency indicator
    lcd.setCursor(6, 1);
    if(frequency < 100) lcd.write(' ');
    lcd.print(frequency, 1);

    // Mono / stereo indicator
    lcd.setCursor(14, 1);
    if(stereo){
      lcd.write(STEREO_CHAR_S);
      lcd.write(STEREO_CHAR_T);
    } else
      lcd.print("  ");
  }
  
  if(bitRead(status, ST_SEARCH)) {   // Is the radio performing an automatic search?
    if(Radio.process_search(buf, searchDirection) == 1) {
      bitWrite(status, ST_SEARCH, 0);
    }
  }

  // Encoder being turned clockwise (+)
  if(bitRead(status, ST_GO_UP)) {
    if(bitRead(status, ST_AUTO) && !bitRead(status, ST_SEARCH)) {
      // Automatic search mode (only processed if the radio is not currently performing a search)
      bitWrite(status, ST_SEARCH, 1);
      searchDirection = TEA5767_SEARCH_DIR_UP;
      Radio.search_up(buf);
      delay(50);
    } else {
      // Manual tuning mode
      if(frequency < 108) {
        frequency += 0.1;
        Radio.set_frequency(frequency);
      }
    }
    bitWrite(status, ST_GO_UP, 0);
  }

  // Encoder being turned counterclockwise (-)
  if(bitRead(status, ST_GO_DOWN)) {
    if(bitRead(status, ST_AUTO) && !bitRead(status, ST_SEARCH)) {
      // Automatic search mode (only processed if the radio is not currently performing a search)
      bitWrite(status, ST_SEARCH, 1);
      searchDirection = TEA5767_SEARCH_DIR_DOWN;
      Radio.search_down(buf);
      delay(50);
    } else {
      // Manual tuning mode
      if(frequency > 88) {
        frequency -= 0.1;
        Radio.set_frequency(frequency);
      }
    }
    bitWrite(status, ST_GO_DOWN, 0);
  }
      
}

