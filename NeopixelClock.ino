/*!
 * @file NeopixelClock
 * 
 * Yet another Neopixel clock. 
 * 
 * Hardware:
 * - Arduino Nano
 * - Real Time Clock DS1307 (Adafruit) - I2C
 * - Ring of 60 5050 Neopixels (Adafruit) - digital pin 12
 * - Ring of 24 5050 Neopixels (Adafruit) - digital pin 11
 * - SSD1306 OLED display 128 x 96 pixels - I2C
 * - Two buttons for input on digital pins 9 and 10
 */

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

/*
 * The Adafruit library for the SSD1305 OLED display uses a LOT of memory. This ASCII 
 * library is limited to just text display and doesn't do any fancy graphics but uses
 * a great deal less memory and has a great set of fonts.
 * 
 * https://github.com/greiman/SSD1306Ascii
 */
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"


/*! 
 * @brief I2C Addess for the SDD1306 OLED display.
 *
 * 0X3C+SA0 - 0x3C or 0x3D
 */
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


// Function pointer typedef
typedef void (*DISP_FUNC_T)(DateTime now, DateTime then, bool init);

/*!
 * @brief Returns true if a button is held down for the specified time.
 * 
 * time_ms should be in the order of 100s of MS. A value less than 100 will
 * return TRUE instantly, I expect.
 * 
 * @param[in] button Digital button pin number.
 * @param[in] time_ms Time period in miliseconds that the button should be held for.
 * 
 * @returns true if the button was held for at least time_ms, otherwise false.
 */
static bool long_button_press(int button, int time_ms) {
  long unsigned start = millis();
  
  while(digitalRead(button) == LOW);
  return (time_ms > (millis() - start)) ? false : true;  
}

/*!
 * @brief Set all the neopixels so that they are off.
 * 
 * @returns None.
 */
static void clear_rings(void) {
  for(int idx=0; idx<RING60_MAX; idx++) ring60.setPixelColor(idx, 0, 0, 0);
  ring60.show();

  for(int idx=0; idx<RING24_MAX; idx++) ring24.setPixelColor(idx, 0, 0, 0);
  ring24.show();  
}


int clr[3]= { 20, 0, 0 };
int sub_idx = 0;
int add_idx = 1;

/*!
 * @brief calculate the next pastel color in the sequence
 * 
 * The 3 element array 'clr' contains Red, Green and Blue colors in range 0..20 for each.
 * Only two colours are displayed at any given time. One is incremented and the other 
 * decremented so that a 20 step gradiant is required to change between one primary 
 * color and the next.
 */
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

  if (init) clear_rings();

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
  if (init) clear_rings();
  
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

  if (init) clear_rings();

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

//    oled.setCursor(40, 20);
//    if (now.second() < 10) oled.print("0");
//    oled.print(now.second());  
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

DISP_FUNC_T disp_func[] = { ShowTime, ArcTime, RingTime };
const int disp_func_max = (int)sizeof(disp_func)/sizeof(DISP_FUNC_T);
char disp_func_names[][11] = {
  "Default   ",
  "Pastel Arc",
  "RGB Ring  "
};

int disp_mode;

DateTime now;
DateTime then;

/*!
 * @brief Configure the Neopixel display mode of the clock.
 * 
 * @ returns On blue button press, returns true if held longer than 1-second,
 *           otherwise false.
 */
bool configure_display() {
  bool red_button_press;
  int row;

  oled.setFont(Arial14); 
  oled.clear();
  oled.println("Set Display");

  while(true) {
    red_button_press = false;
    oled.setCursor(0, oled.fontRows());
    oled.print(disp_func_names[disp_mode]);
    oled.clearToEOL();
    disp_func[disp_mode](now, then, true);

    while(!red_button_press) {
      if (digitalRead(BLU_BTTN_PIN) == LOW) {
        return (long_button_press(BLU_BTTN_PIN, 1000)) ? true : false;
      }
      else if (digitalRead(RED_BTTN_PIN) == LOW) {
        red_button_press = long_button_press(RED_BTTN_PIN, 10);
        disp_mode = (disp_mode + 1) % disp_func_max;
      }
      now = rtc.now();
      disp_func[disp_mode](now, then, false);
      then = now;
      delay(100);    
    }
  }
}

int oled_disp_mode;
char oled_mode_names[][11] = {
  "Off       ",
  "Date      ",
  "Seconds   "
};
#define oled_mode_max 3

/*!
 * @brief Configure the OLED display mode
 * 
 * OLED display options
 * - Display off
 * - Display date
 * - Display seconds
 * 
 * @returns On blue button press, returns true if held longer than 1-second
 *           otherwise false.
 */
bool configure_oled() {
  bool red_button_press;
  int row;

  oled.setFont(Arial14); 
  oled.clear();
  oled.println("Set OLED");

  while(true) {
    red_button_press = false;
    oled.setCursor(0, oled.fontRows());
    oled.print(oled_mode_names[oled_disp_mode]);
    oled.clearToEOL();
    //disp_func[disp_mode](now, then, true);

    while(!red_button_press) {
      if (digitalRead(BLU_BTTN_PIN) == LOW) {
        return (long_button_press(BLU_BTTN_PIN, 1000)) ? true : false;
      }
      else if (digitalRead(RED_BTTN_PIN) == LOW) {
        red_button_press = long_button_press(RED_BTTN_PIN, 10);
        oled_disp_mode = (oled_disp_mode + 1) % oled_mode_max;
      }
      now = rtc.now();
      disp_func[disp_mode](now, then, false);
      then = now;
      delay(100);    
    }
  }  
}

/*!
 * @brief Configure the the aspects of the Neopixelclock
 * 
 * Configures the following aspects in the following order
 * - Display mode 
 * - OLED display mode
 * - Date 
 * - Time
 * If the blue button is held for more than 1-second at any point the
 * whole configuration function is exited.
 * 
 * @returns None.
 */
void configure() {
  // Wait for the Blue button to be released.
  while (digitalRead(BLU_BTTN_PIN) == LOW);

  if (configure_display()) return;
  if (configure_oled()) return;
}


void setup() {
  PRINT_INIT( 9600 );
  PRINTLN("Setup started.");

  disp_mode = 0;
  oled_disp_mode = 0;

  // Intialise the Real Time Clock.
  if (!rtc.begin()) {
    PRINTLN("Couldn't initialise the RTC.");
    while(1);
  }

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  ring60.begin();
  ring24.begin();

  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  //oled.set400kHz();
  oled.set2X();  
  oled.setFont(lcdnums14x24);
  //oled.setFont(fixednums15x31);
  oled.clear();

  now = then = rtc.now();
  disp_func[disp_mode](now, then, true);

  pinMode(RED_BTTN_PIN, INPUT_PULLUP);
  pinMode(BLU_BTTN_PIN, INPUT_PULLUP);
  PRINTLN("Setup completed.");
}


void loop() {

    if (digitalRead(BLU_BTTN_PIN) == LOW)  {
      configure();
      oled.clear();
    }
        
    now = rtc.now();
    disp_func[disp_mode](now, then, false);
    then = now;    
}
