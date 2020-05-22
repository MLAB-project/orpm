String githash = "0";
String FWversion = "ORPM";

/*

 Converter for Optical RPM sensor to RS232

ISP
---
PD0     RX
PD1     TX
RESET#  through 50M capacitor to RST#

SDcard
------
DAT3   SS   4 B4
CMD    MOSI 5 B5
DAT0   MISO 6 B6
CLK    SCK  7 B7

ANALOG
------
+      A0  PA0
-      A1  PA1
RESET  0   PB0

LED
---
LED_red  23  PC7         // LED for Dasa

Time Synchronisation
--------------------
SYNC0   18  PC2
SYNC1   19  PC3


                     Mighty 1284p    
                      +---\/---+
           (D 0) PB0 1|        |40 PA0 (AI 0 / D24)
           (D 1) PB1 2|        |39 PA1 (AI 1 / D25)
      INT2 (D 2) PB2 3|        |38 PA2 (AI 2 / D26)
       PWM (D 3) PB3 4|        |37 PA3 (AI 3 / D27)
    PWM/SS (D 4) PB4 5|        |36 PA4 (AI 4 / D28)
      MOSI (D 5) PB5 6|        |35 PA5 (AI 5 / D29)
  PWM/MISO (D 6) PB6 7|        |34 PA6 (AI 6 / D30)
   PWM/SCK (D 7) PB7 8|        |33 PA7 (AI 7 / D31)
                 RST 9|        |32 AREF
                VCC 10|        |31 GND
                GND 11|        |30 AVCC
              XTAL2 12|        |29 PC7 (D 23)
              XTAL1 13|        |28 PC6 (D 22)
      RX0 (D 8) PD0 14|        |27 PC5 (D 21) TDI
      TX0 (D 9) PD1 15|        |26 PC4 (D 20) TDO
RX1/INT0 (D 10) PD2 16|        |25 PC3 (D 19) TMS
TX1/INT1 (D 11) PD3 17|        |24 PC2 (D 18) TCK
     PWM (D 12) PD4 18|        |23 PC1 (D 17) SDA
     PWM (D 13) PD5 19|        |22 PC0 (D 16) SCL
     PWM (D 14) PD6 20|        |21 PD7 (D 15) PWM
                      +--------+
*/

/*
// Compiled with: Arduino 1.8.9
// MightyCore 2.0.2 https://mcudude.github.io/MightyCore/package_MCUdude_MightyCore_index.json
Fix old bug in Mighty SD library
~/.arduino15/packages/MightyCore/hardware/avr/2.0.2/libraries/SD/src/SD.cpp:
boolean SDClass::begin(uint32_t clock, uint8_t csPin) {
  if(root.isOpen()) root.close();
*/

#include <SD.h>             // Revised version from MightyCore
#include "wiring_private.h"
#include <Wire.h>           

#define LED_red   23   // PC7
#define GPSpower  26   // PA2
#define SDpower1  1    // PB1
#define SDpower2  2    // PB2
#define SDpower3  3    // PB3
#define SS        4    // PB4
#define MOSI      5    // PB5
#define MISO      6    // PB6
#define SCK       7    // PB7
#define INT       20   // PC4
#define TP       21   // PC5

uint32_t serialhash = 0;


// Read Analog Differential without gain (read datashet of ATMega1280 and ATMega2560 for refference)
// Use analogReadDiff(NUM)
//   NUM  | POS PIN             | NEG PIN           |   GAIN
//  0 | A0      | A1      | 1x
//  1 | A1      | A1      | 1x
//  2 | A2      | A1      | 1x
//  3 | A3      | A1      | 1x
//  4 | A4      | A1      | 1x
//  5 | A5      | A1      | 1x
//  6 | A6      | A1      | 1x
//  7 | A7      | A1      | 1x
//  8 | A8      | A9      | 1x
//  9 | A9      | A9      | 1x
//  10  | A10     | A9      | 1x
//  11  | A11     | A9      | 1x
//  12  | A12     | A9      | 1x
//  13  | A13     | A9      | 1x
//  14  | A14     | A9      | 1x
//  15  | A15     | A9      | 1x
#define PIN 0
uint8_t analog_reference = INTERNAL2V56; // DEFAULT, INTERNAL, INTERNAL1V1, INTERNAL2V56, or EXTERNAL


void setup()
{
  //pinMode(SDpower1, OUTPUT);  // SDcard interface
  //pinMode(SDpower2, OUTPUT);     
  //pinMode(SDpower3, OUTPUT);     
  //pinMode(SS, OUTPUT);     
  //pinMode(MOSI, INPUT);     
  //pinMode(MISO, INPUT);     
  //pinMode(SCK, OUTPUT);  
  //pinMode(RESET, OUTPUT);   // reset signal for peak detetor

  DDRB = 0b10011110;   // SDcard Power OFF
  PORTB = 0b00000001;  
  DDRA = 0b11111100;
  PORTA = 0b00000000;  // Initiating ports
  DDRC = 0b11101100;
  PORTC = 0b00000000;  
  DDRD = 0b11111100;
  PORTD = 0b00000000;  

  Wire.setClock(100000);

  // Open serial communications
  Serial.begin(9600);
  Serial1.begin(9600);

  Serial.println("#Cvak...");
  
  pinMode(LED_red, OUTPUT);
  digitalWrite(LED_red, LOW);  
  
  for(int i=0; i<5; i++)  
  {
    delay(50);
    digitalWrite(LED_red, HIGH);  // Blink 
    delay(50);
    digitalWrite(LED_red, LOW);  
  }

  Serial.println("#Hmmm...");

  // make a string for device identification output
  String dataString = "$AIRDOS," + FWversion + "," + githash + ","; // FW version and Git hash

  if (digitalRead(17)) // Protection against sensor mallfunction 
  {
    Wire.beginTransmission(0x58);                   // request SN from EEPROM
    Wire.write((int)0x08); // MSB
    Wire.write((int)0x00); // LSB
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)0x58, (uint8_t)16);    
    for (int8_t reg=0; reg<16; reg++)
    { 
      uint8_t serialbyte = Wire.read(); // receive a byte
      if (serialbyte<0x10) dataString += "0";
      dataString += String(serialbyte,HEX);    
      serialhash += serialbyte;
    }
  }
  else
  {
    dataString += "NaN";    
  }
    
  digitalWrite(LED_red, HIGH);  // Blink for Dasa
  Serial.println(dataString);  // print SN to terminal 
  digitalWrite(LED_red, LOW);          
}

#define WORDTIME  300
#define BYTETIME  20

void loop()
{
  uint8_t number = 0;

  // World gap
  while(true)
  {
    uint16_t loops = 0;
    while(!(PINC& 0b00010000))
    {
      loops++;
    }
    if(loops > WORDTIME) break;
  }  
  while(!(PINC& 0b00010000));
  //PORTC = 0b00100000;  
  //PORTC = 0b00000000;  

  // Preamble 
  number = 0;
  while(true)
  {
    while((PINC& 0b00010000)); 
    uint8_t loops = 0;
    while(!(PINC& 0b00010000)) 
    {
      loops++;
      if(loops > BYTETIME) break;
    }
    number++;
    if(loops > BYTETIME) break;
  }  
  if(number!=12) return;
  while(!(PINC& 0b00010000));

  // 4th byte
  number = 0;
  while(true)
  {
    while((PINC& 0b00010000));
    uint16_t loops = 0;
    while(!(PINC& 0b00010000))
    {
      loops++;
      if(loops > BYTETIME) break;
    }
    number++;
    if(loops > BYTETIME) break;
  }  
  uint8_t n1000 = 10 - number;
  while(!(PINC& 0b00010000));

  // 3rd byte
  number = 0;
  while(true)
  {
    while((PINC& 0b00010000));
    uint16_t loops = 0;
    while(!(PINC& 0b00010000))
    {
      loops++;
      if(loops > BYTETIME) break;
    }
    number++;
    if(loops > BYTETIME) break;
  }  
  uint8_t n100 = 10 - number;
  while(!(PINC& 0b00010000));

  // 2nd byte
  number = 0;
  while(true)
  {
    while((PINC& 0b00010000));
    uint16_t loops = 0;
    while(!(PINC& 0b00010000))
    {
      loops++;
      if(loops > BYTETIME) break;
    }
    number++;
    if(loops > BYTETIME) break;
  }  
  uint8_t n10 = 10 - number;
  while(!(PINC& 0b00010000));

  // 1st byte
  number = 0;
  while(true)
  {
    while((PINC& 0b00010000));
    uint16_t loops = 0;
    while(!(PINC& 0b00010000))
    {
      loops++;
      if(loops > BYTETIME) break;
    }
    number++;
    if(loops > BYTETIME) break;
  }  
  uint8_t n1 = 10 - number;

  Serial.println(1000 * n1000 + 100 * n100 + 10 * n10 + n1); 

}
