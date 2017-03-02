// Arduino Nano
// Real Time Clock DS1307
// Ring of 60 5050 Neopixels
// RIng of 24 5050 NeoPixels
// SDD1306 OLED i2c display 128 x 96

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

#define RED_BTTN_PIN 10
#define BLU_BTTN_PIN 9

RTC_DS1307 rtc;
Adafruit_NeoPixel ring60 = Adafruit_NeoPixel(RING60_MAX, RING60_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ring24 = Adafruit_NeoPixel(RING24_MAX, RING24_PIN, NEO_GRB + NEO_KHZ800);
SSD1306AsciiWire oled;

enum LoopMode { setup_mode, show_time_mode };
LoopMode mode = show_time_mode;

int clr[3]= { 20, 0, 0 };
int sub_idx = 0;
int add_idx = 1;

static void next_color( void ) {
    if ( clr[add_idx] < 20) {
      clr[add_idx]++;
      clr[sub_idx]--;
    }
    else {
      sub_idx = add_idx;
      add_idx = (add_idx+1) % 3;
    }
}

// ArcTime
static void ArcTime(DateTime now, DateTime then, bool init=false) {
  int pixel;

  if (init || ( now.minute() != then.minute() ) ) {
    pixel = now.minute();
    if (pixel == 0) {
      for (int idx=0; idx<RING60_MAX; idx++) ring60.setPixelColor(idx, 0, 0 ,0);
    }
    else {
      for (int idx=0; idx<=pixel; idx++) {
        ring60.setPixelColor( idx, clr[0], clr[1], clr[2]);
        next_color();
      }
    }    
    ring60.show();
  }

  if (init || ( now.hour() != then.hour() ) ) {
    if (now.hour() == 0) {
      for (int idx=0; idx<RING24_MAX; idx) ring24.setPixelColor(idx, 0, 0, 0);
    }
    else {
      pixel = ( (now.hour() > 12) ? now.hour()-12 : now.hour() ) * 2;
      for (int idx=0; idx<=pixel; idx++) {
        ring24.setPixelColor( idx, clr[0], clr[1], clr[2]);
        next_color();
      }
    }
    ring24.show();
  }
}


static void RingTime(DateTime now, DateTime then, bool init=false) {
  if (init | (now.second() != then.second()) ) {
    int red, green, blue;

    for (int idx=0; idx<RING60_MAX; idx++) {
      if (now.hour() < 12) { 
        blue = (idx <= now.second()) ? 30 : 0;
        red = 0;
      }
      else {
        blue = 0;
        red = (idx <= now.second()) ? 30: 0;
      }
      green = (idx <= now.minute()) ? 30 : 0;
      ring60.setPixelColor( idx, red, green, blue );
    }
    ring60.show();
  }

  if (init | (now.hour() != then.hour()) ) {
    int red, blue;
    int hr = ( (now.hour() > 12) ? now.hour() - 12 : now.hour() ) * 2;

    for (int idx=0; idx<RING24_MAX; idx++) {
      if (now.hour() < 12) {
        red = 30;
        blue = 0;
      } 
      else {
        red = 0;
        blue = 30;
      }
      if (idx < hr)
        ring24.setPixelColor(idx, red, 0, blue);
      else
        ring24.setPixelColor(idx, 0, 0, 0);
    }
    ring24.show();
  }
    
}


static void ShowTime(DateTime now, DateTime then, bool init=false) {

  int hr = (now.hour() > 12) ? now.hour()-12 : now.hour();
  int pix_hour = (hr * 5);

  if (init || (now.second() != then.second() ) ) {
    for (int idx=0; idx<RING60_MAX; idx++) {
      if ( (idx % 5) == 0 )
        ring60.setPixelColor( idx, 10, 10, 10 );
      else
        ring60.setPixelColor( idx, 0, 0 , 0 );
    }
//    ring60.setPixelColor( pix_hour, 30, 0, 0 );
    ring60.setPixelColor( now.minute(), 0, 30, 0 );
    ring60.setPixelColor( now.second(), 0, 0, 30 );
  
    ring60.show();

    oled.setCursor(40, 20);
    if (now.second() < 10) oled.print("0");
    oled.print(now.second());  
  }

  if (init || (now.minute() != then.minute() ) ) {
    for (int idx=0; idx<RING24_MAX; idx++) {
      ring24.setPixelColor( idx, 0, 0, 0 );
    }
  
    hr = (now.hour() > 12) ? now.hour()-12 : now.hour();
    pix_hour = hr * 2;
    ring24.setPixelColor(pix_hour, 30, 0, 0);
    ring24.show();
  }
}

DateTime now;
DateTime then;

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

  now = then = rtc.now();
  RingTime(now, then, true);

  pinMode(RED_BTTN_PIN, INPUT_PULLUP);
  pinMode(BLU_BTTN_PIN, INPUT_PULLUP);
  PRINTLN("Setup completed.");
}


void loop() {
  
    now = rtc.now();
  
    RingTime(now, then);
  
    then = now;    
}
