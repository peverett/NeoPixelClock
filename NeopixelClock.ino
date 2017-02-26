// Arduino Nano
// Real Time Clock DS1307
// Ring of 60 5050 Neopixel

#define DEBUG_PRINT
#ifdef DEBUG_PRINT
#define PRINT_INIT(X) Serial.begin((X))
#define PRINT(X) Serial.print(X)
#define PRINTLN(X) Serial.println(X)
#else 
#define PRINT_INIT(X)
#define PRINT(X)
#define PRINTLN(X)
#endif

#include <Wire.h>
#include "RTClib.h"
#include "Adafruit_NeoPixel.h"

#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"


// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS 0x3C


#define RING60_MAX 60
#define RING60_PIN 12

#define RING24_MAX 24
#define RING24_PIN 11

RTC_DS1307 rtc;
Adafruit_NeoPixel ring60 = Adafruit_NeoPixel(RING60_MAX, RING60_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ring24 = Adafruit_NeoPixel(RING24_MAX, RING24_PIN, NEO_GRB + NEO_KHZ800);
SSD1306AsciiWire oled;

static void ShowTime(void) {
  DateTime now = rtc.now();
  PRINT(now.hour());
  PRINT(":"); 
  PRINT(now.minute());
  PRINT(":"); 
  PRINTLN(now.second());

  int hr = (now.hour() > 12) ? now.hour()-12 : now.hour();
  int pix_hour = (hr * 5);

  for (int idx=0; idx<RING60_MAX; idx++) {
    if ( (idx % 5) == 0 )
      ring60.setPixelColor( idx, 10, 10, 10 );
    else
      ring60.setPixelColor( idx, 0, 0 , 0 );
  }

  ring60.setPixelColor( pix_hour, 30, 0, 0 );
  ring60.setPixelColor( now.minute(), 0, 30, 0 );
  ring60.setPixelColor( now.second(), 0, 0, 30 );

  ring60.show();

  for (int idx=0; idx<RING24_MAX; idx++) {
    ring24.setPixelColor( idx, 0, 0, 0 );
  }

  hr = (now.hour() > 12) ? now.hour()-12 : now.hour();
  pix_hour = hr * 2;
  ring24.setPixelColor(pix_hour, 30, 0, 0);
  ring24.show();

  oled.setCursor(40, 20);
  if (now.second() < 10) oled.print("0");
  oled.print(now.second());
}


void setup() {
  PRINT_INIT( 9600 );
  PRINTLN("Setup started.");

  // Intialise the Real Time Clock.
  if (!rtc.begin()) {
    PRINTLN("Couldn't initialise the RTC.");
    while(1);
  }

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  ring60.begin();
  for(int idx=0; idx<RING60_MAX; idx++) ring60.setPixelColor(idx, 0, 0, 0);
  ring60.show();

  ring24.begin();
  for(int idx=0; idx<RING24_MAX; idx++) ring24.setPixelColor(idx, 0, 0, 0);
  ring24.show();

  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  //oled.set400kHz();
  oled.set2X();  
  oled.setFont(lcdnums14x24);
  //oled.setFont(fixednums15x31);
  oled.clear();

  PRINTLN("Setup completed.");
}

void loop() {
  ShowTime();
  delay(200);
}
