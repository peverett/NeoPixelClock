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

#include <Arduino.h>
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

int disp_mode;

DateTime now;
DateTime then;

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
 * @brief Returns true if a button is held down for up to the specified time.
 * 
 * The fastest button quick press is probably about 200 to 300 ms. This can be
 * used to return true constantly while the button is held down.
 * 
 * @param[in] button Digital button pin number.
 * @param[in] time_ms Time period in miliseconds that the button should be held for.
 * 
 * @returns None.
 */
static void short_button_press(int button, int time_ms) {
  long unsigned start = millis();
  
  while(digitalRead(button) == LOW && (time_ms > millis() - start));
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

/*!
 * @brief OLED displays nothing - is blank!
 *   
 * @returns void.
 */
static void oled_off(DateTime now, DateTime then, bool init=false) {
  if (init) oled.clear();
}

/*!
 * @brief OLED displays seconds in 8-segment display font!
 *  
 * @param now DateTime just read from RTC. 
 * @param then Previous DateTime before current reading.
 * @param init True if initialising this display mode.
 * 
 * @returns void.
 */
static void oled_seconds(DateTime now, DateTime then, bool init=false) {
  if (init) {
    // Use the lcdnums font.
    oled.setFont(lcdnums14x24);
    oled.clear();     
  }
  if (init || (now.second() != then.second())) {
    oled.setCursor(40, 20);
    if (now.second() < 10) oled.print("0");
    oled.print(now.second());
  }  
}

const char week_days[][11] = {
  "   Sunday",
  "   Monday",
  "  Tuesday",
  "Wednesday",
  " Thursday",
  "   Friday",
  " Saturday"
};

/*!
 * @brief OLED Day of week, year/month/day
 *  
 * @param now DateTime just read from RTC. 
 * @param then Previous DateTime before current reading.
 * @param init True if initialising this display mode.
 * 
 * @returns void.
 */
static void oled_date(DateTime now, DateTime then, bool init=false) {
  if (init) {
    oled.setFont(Arial14); 
  }
  if (init || (now.day() != then.day())) {
    oled.clear();       // Draw fresh every day.
    oled.println(week_days[now.dayOfTheWeek()]);
    oled.print(now.year());
    oled.print("/");
    if (now.month() < 10) oled.print("0");
    oled.print(now.month());
    oled.print("/");
    if (now.day() < 10) oled.print("0");
    oled.print(now.day());
  }  
}

DISP_FUNC_T oled_func[] = { oled_off, oled_seconds, oled_date };
const int oled_mode_max = (int)sizeof(oled_func)/sizeof(DISP_FUNC_T);
int oled_mode;
char oled_mode_names[][11] = {
  "Off       ",
  "Seconds   ",
  "Date      "
 };


/*!
 * @brief convert 24-hour clock time to 12-hour.
 * 
 * @param[in] twenty_four Twenty four hour clock time, range 0..23.
 */
inline uint8_t TwentyFourToTwelve(uint8_t twenty_four) {
  return (twenty_four >= 12) ? twenty_four - 12 : twenty_four;
}

/*!
 * @brief Neopixel Display Base Class
 * 
 * In the HW there are two NeoPixel rings
 * - One ring with 24-NeoPixel - used for hours.
 * - One ring with 60-NeoPixel - used for minutes and seconds.
 */
class BaseTimeDisplay {
public:
  BaseTimeDisplay(Adafruit_NeoPixel &r24, Adafruit_NeoPixel &r60) :
    ring24( r24 ),
    ring60( r60 )
    {};

  /*!
   * @brief Display the current time passed in 'tn', intialising the display
   * 
   * Initialises the display for time now 'tn', setting all NeoPixels. This
   * is a virtual function in the base class and must be overridden in 
   * the derived classes/
   * 
   * @param[in] tn Time now in DateTime format.
   */
  virtual void Display(const DateTime tn) {};

  /*!
   * @brief Update the display for the difference between 'tn' - time now - and
   * 'tt' - time then. 
   * 
   * The point of using Update is that it should only change as much of the 
   * Neopixel display as is required to show the new time, without setting
   * all the Neopixels. For example. If only minutes have changed, only update
   * the 60-NeoPixel ring, don't change the 24-NeoPixel ring.
   * 
   * @param[in] tn Time now in DateTime format.
   * @param[in] tt Time then in DateTime format.
   */
   virtual void Update(const DateTime tn, const DateTime tt) {};
      
protected:
  Adafruit_NeoPixel ring24;
  Adafruit_NeoPixel ring60;
};

/*!
 * @brief Display time as growing arcs.
 * 
 * - Hours are displayed as an Arc starting from the 12 o'clock postion.
 * - Likewise minutes, and seconds. 
 * - The colour of minutes and seconds are combined so that when the arcs overlap
 *   the NeoPixel color is that of both seconds and minutes.
 * - The colours change depending on whether the time is before or after mid-day.
 */
class ArcTimeDisplay: public BaseTimeDisplay {
public:
  ArcTimeDisplay(Adafruit_NeoPixel &r24, Adafruit_NeoPixel &r60)
    : BaseTimeDisplay(r24, r60)
    {};

  void Display(const DateTime tn) {
    uint8_t hr = TwentyFourToTwelve(tn.hour()) * 2;

    int red, green, blue;

    for (int idx=0; idx<RING60_MAX; idx++) {
      if (tn.hour() < 12) { 
        blue = (idx <= tn.second()) ? 30 : 0;
        red = 0;
      }
      else {
        blue = 0;
        red = (idx <= tn.second()) ? 30: 0;
      }
      green = (idx <= tn.minute()) ? 30 : 0;
      ring60.setPixelColor(idx, red, green, blue );
    }

    red = (tn.hour() < 12) ? 30 : 0;
    blue = (tn.hour() < 12) ? 0: 30;
    
    /* At mid-night no hours are shown. */
    if (tn.hour() == 0) {
      for (int idx=0; idx<=RING24_MAX; idx++) {
        ring24.setPixelColor(idx, 0, 0, 0); 
      }     
    }
    /* At mid-day the full ring is shown in blue. */
    else if (tn.hour() == 12) {
      for (int idx=0; idx<=RING24_MAX; idx++) {
        ring24.setPixelColor(idx, red, 0, blue);
      }
    }
    else {
      for (int idx=0; idx<RING24_MAX; idx++) {
        if (idx <= hr)
          ring24.setPixelColor(idx, red, 0, blue);
        else
          ring24.setPixelColor(idx, 0, 0, 0);
      }
    }
    ring24.show();
    ring60.show();
  };

  /* Only updates seconds and minutes. On minute transition, just redraw the whole 
   * display.
   */
  void Update(const DateTime tn, const DateTime tt) {
    if (tn.minute() != tt.minute()) {
      Display(tn);
    } 
    else if (tn.second() != tt.second()) {
      int red, green, blue;

      blue = (tn.hour() < 12) ? 30 : 0;
      red = (tn.hour()< 12) ? 0 : 30;
      green = (tn.second() <= tn.minute()) ? 30 : 0;

      ring60.setPixelColor(tn.second(), red, green, blue);
      ring60.show();
    }
  };
};


/*!
 * @brief Displays pastel rings of NeoPixels with the hours and minutes 
 *        indicated by blinking the hour and minute NeoPixels.
 *        
 * - Both 60 and 24-Neopixel ring are illuminated except for time indicating
 *   NeoPixel, which blinks.
 * - The colors on both rings rotatate fully, once per minute (approx).
 */
class PastelTimeDisplay: public BaseTimeDisplay {
public:
  PastelTimeDisplay(Adafruit_NeoPixel &r24, Adafruit_NeoPixel &r60)
    : BaseTimeDisplay(r24, r60)
    {};

  void Display(const DateTime tn) {
    int clr[3] = {20, 0, 0};
    int sub_idx = 0;
    int add_idx = 1;
    int step_val = 2;
    uint8_t hr = TwentyFourToTwelve(tn.hour()) * 2;

    for (int idx=0; idx < RING60_MAX; idx++) {
      ring60.setPixelColor(idx, clr[0], clr[1], clr[2]);
      if (clr[add_idx] == 20) {
        sub_idx = add_idx;
        add_idx = (add_idx+1) % 3;      
      }
      clr[add_idx]++;
      clr[sub_idx]--;      
    }

    for (int idx=0; idx<RING24_MAX; idx++) {
      ring24.setPixelColor(idx, clr[0], clr[1], clr[2]);
      if (clr[add_idx] == 20) {
        sub_idx = add_idx;
        add_idx = (add_idx+1) % 3;      
      }
      clr[add_idx]+=step_val;
      clr[sub_idx]-=step_val;
      step_val = (step_val == 2) ? 3 : 2;
    }

    hour_clr = ring24.getPixelColor(hr);
    minute_clr = ring60.getPixelColor(tn.minute());

    ring24.setPixelColor(hr, 0);
    ring60.setPixelColor(tn.minute(), 0);

    ring60.show();
    ring24.show();

    minute_tmr = millis() + mn_delay;
    hour_tmr = millis() + hr_delay;
    blink_tmr = millis() + bk_delay;
    led_off = true;
  };

  void Update(const DateTime tn, const DateTime tt) {
    int tn_hr = TwentyFourToTwelve(tn.hour()) * 2;
    int tt_hr = TwentyFourToTwelve(tt.hour()) * 2;
    uint32_t temp;

    ring24.setPixelColor(tt_hr, hour_clr);
    ring60.setPixelColor(tt.minute(), minute_clr);

    if (millis() > minute_tmr) {
        temp = ring60.getPixelColor(RING60_MAX-1);
        for (int idx=RING60_MAX-1; idx > 0; idx--) {
        ring60.setPixelColor(idx, ring60.getPixelColor(idx-1));
      }
      ring60.setPixelColor(0, temp); 
      minute_tmr = millis() + mn_delay;
    }

    if (millis() > hour_tmr) {
      temp = ring24.getPixelColor(RING24_MAX-1);
      for (int idx=RING24_MAX-1; idx > 0; idx--) {
        ring24.setPixelColor(idx, ring24.getPixelColor(idx-1));
      }
      ring24.setPixelColor(0, temp);
      hour_tmr = millis() + hr_delay;
    }

    hour_clr = ring24.getPixelColor(tn_hr);
    minute_clr = ring60.getPixelColor(tn.minute());
    if (led_off) {
        ring24.setPixelColor(tn_hr, 0);
        ring60.setPixelColor(tn.minute(), 0);
    }
    if (millis() > blink_tmr) {
      led_off = (led_off) ? false : true;
      blink_tmr = millis() + bk_delay;
    }

    ring24.show();
    ring60.show();
  };

private:
  uint32_t minute_clr;
  uint32_t hour_clr;
  unsigned long minute_tmr;
  unsigned long hour_tmr;
  unsigned long blink_tmr;
  bool  led_off;

  const int mn_delay = 1000;
  const int hr_delay = 2500;
  const int bk_delay = 500;
};


/*!
 * @brief Simulates an analogue clock desplay
 * 
 * - The 'hour' positions are highlighted by illuminating them white.
 * - The hours are shown on the 24-NeoPixel ring as red.
 * - Minutes are shown on the 60-NeoPixel ring as green.
 * - Seconds are shown on the 60-NeoPixel ring as blue.
 * 
 * Minutes occlude the hour markers, and seconds occlude both of those.
 */
class AnalogTimeDisplay: public BaseTimeDisplay {
public:
  AnalogTimeDisplay(Adafruit_NeoPixel &r24, Adafruit_NeoPixel &r60)
    : BaseTimeDisplay(r24, r60)
    {};

  void Display(const DateTime tn) {
    uint8_t hr = TwentyFourToTwelve(tn.hour()) * 2;

    for (int idx=0; idx < RING60_MAX; idx++) {
      if ((idx % 5) == 0) 
        ring60.setPixelColor(idx, 10, 10, 10);
      else
        ring60.setPixelColor(idx, 0, 0, 0);
    }
    ring60.setPixelColor(tn.minute(), 0, 30, 0);
    ring60.setPixelColor(tn.second(), 0, 0, 30);

    for (int idx=0; idx<RING24_MAX; idx++) {
      ring24.setPixelColor(idx, 0, 0, 0);
    }
    ring24.setPixelColor(hr, 30, 0, 0);

    ring60.show();
    ring24.show();
  };

  void Update(const DateTime tn, const DateTime tt) {
    int tn_hr = TwentyFourToTwelve(tn.hour()) * 2;
    int tt_hr = TwentyFourToTwelve(tt.hour()) * 2;
    bool show60 = false;
    
    if (tn_hr != tt_hr) {
      ring24.setPixelColor(tn_hr, 30, 0, 0);
      ring24.setPixelColor(tt_hr, 0, 0, 0);
      ring24.show();
    }

    if (tn.minute() != tt.minute()) {
      if ((tt.minute() % 5) == 0)
        ring60.setPixelColor(tt.minute(), 10, 10, 10);
      else
        ring60.setPixelColor(tt.minute(), 0, 0, 0);

      ring60.setPixelColor(tn.minute(), 0, 30, 0);
      show60=true;
    }
    
    if (tn.second() != tt.second()) {
      if (tt.second() == tn.minute())
        ring60.setPixelColor(tn.minute(), 0, 30, 0);
      else if ((tt.second() % 5) == 0)
        ring60.setPixelColor(tt.second(), 10, 10, 10);
      else
        ring60.setPixelColor(tt.second(), 0, 0, 0);

      ring60.setPixelColor(tn.second(), 0, 0, 30);
      show60=true;
    }

    if (show60) ring60.show();
  };
};

AnalogTimeDisplay analog_time(ring24, ring60);
ArcTimeDisplay    arc_time(ring24, ring60);
PastelTimeDisplay pastel_time(ring24, ring60);

BaseTimeDisplay *time_display[] = { &analog_time, &arc_time, &pastel_time };

const int disp_mode_max = sizeof(time_display) / sizeof(BaseTimeDisplay*);

char disp_mode_names[][11] = {
  "Analog   ",
  "RGB Arc  ",
  "Pastel   "
};


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
    oled.print(disp_mode_names[disp_mode]);
    oled.clearToEOL();

    while(!red_button_press) {
      if (digitalRead(BLU_BTTN_PIN) == LOW) {
        return (long_button_press(BLU_BTTN_PIN, 1000)) ? true : false;
      }
      else if (digitalRead(RED_BTTN_PIN) == LOW) {
        red_button_press = long_button_press(RED_BTTN_PIN, 10);
        disp_mode = (disp_mode + 1) % disp_mode_max;
        time_display[disp_mode]->Display(now);
      }
      now = rtc.now();
      time_display[disp_mode]->Update(now, then);
      then = now;
      delay(100);    
    }
  }
}

/*!
 * @brief Configure the OLED display mode
 * 
 * OLED display options
 * - Display off
 * - Display seconds
 * - Display date
 * 
 * @returns On blue button press, returns true if held longer than 1-second
 *           otherwise false.
 */
bool configure_oled() {
  bool red_button_press;

  oled.setFont(Arial14); 
  oled.clear();
  oled.println("Set OLED");

  while(true) {
    red_button_press = false;
    
    oled.setCursor(0, oled.fontRows());
    oled.print(oled_mode_names[oled_mode]);
    oled.clearToEOL();

    while(!red_button_press) {
      if (digitalRead(BLU_BTTN_PIN) == LOW) {
        return (long_button_press(BLU_BTTN_PIN, 1000)) ? true : false;
      }
      else if (digitalRead(RED_BTTN_PIN) == LOW) {
        red_button_press = long_button_press(RED_BTTN_PIN, 10);
        oled_mode = (oled_mode + 1) % oled_mode_max;
      }
      now = rtc.now();
      time_display[disp_mode]->Update(now, then);
      then = now;
      delay(100);    
    }
  }  
}

/*!
 * @brief Configure the time - hours.
 * 
 * @ returns On blue button press, returns true if held longer than 1-second,
 *           otherwise false.
 */
bool configure_time_hour() {
  bool red_button_press;
  int row;
  uint8_t hour;

  oled.setFont(Arial14); 
  oled.clear();
  oled.println("Set Hours");

  now = rtc.now();
  while(true) {
    red_button_press = false;
    oled.setCursor(0, oled.fontRows());
    oled.print(now.hour());
    oled.clearToEOL();

    while(!red_button_press) {
      if (digitalRead(BLU_BTTN_PIN) == LOW) {
        rtc.adjust(now);
        return (long_button_press(BLU_BTTN_PIN, 1000)) ? true : false;
      }
      else if (digitalRead(RED_BTTN_PIN) == LOW) {
        red_button_press = long_button_press(RED_BTTN_PIN, 10);
        hour = (now.hour() + 1) % 24;
        now = DateTime(
            now.year(), 
            now.month(), 
            now.day(), 
            hour, 
            now.minute(), 
            0
          );
      }
      time_display[disp_mode]->Update(now, then);
      then = now;
      delay(100);    
    }
  }
}

/*!
 * @brief Configure the time - minutes.
 * 
 * @ returns On blue button press, returns true if held longer than 1-second,
 *           otherwise false.
 */
bool configure_time_minute() {
  bool red_button_press;
  int row;
  uint8_t minute;

  oled.setFont(Arial14); 
  oled.clear();
  oled.println("Set Minute");

  now = rtc.now();
  while(true) {
    oled.setCursor(0, oled.fontRows());
    oled.print(now.minute());
    time_display[disp_mode]->Display(now);

    oled.clearToEOL();
      if (digitalRead(BLU_BTTN_PIN) == LOW) {
        rtc.adjust(now);
        return (long_button_press(BLU_BTTN_PIN, 1000)) ? true : false;
      }
      else if (digitalRead(RED_BTTN_PIN) == LOW) {
        short_button_press(RED_BTTN_PIN, 200);
        minute = (now.minute() + 1) % 60;
        then = now;
        now = DateTime(
            now.year(), 
            now.month(), 
            now.day(), 
            now.hour(), 
            minute, 
            0
          );
        time_display[disp_mode]->Display(now);
      }
      // time_display[disp_mode]->Update(now, then);
      delay(100);    
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
  if (configure_time_hour()) return;
  if (configure_time_minute()) return;
  then = now;
}

void setup() {
  PRINT_INIT( 9600 );
  PRINTLN("Setup started.");

  disp_mode = 0;
  oled_mode = 0;

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
  oled.set2X();     // Every font is double sized.
  oled.clear();

  now = then = rtc.now();
  time_display[disp_mode]->Display(now);
  oled_func[oled_mode](now, then, true);

  pinMode(RED_BTTN_PIN, INPUT_PULLUP);
  pinMode(BLU_BTTN_PIN, INPUT_PULLUP);
  PRINTLN("Setup completed.");
}


void loop() {

    if (digitalRead(BLU_BTTN_PIN) == LOW)  {
      configure();
      oled_func[oled_mode](now, then, true);
    }
        
    now = rtc.now();
    time_display[disp_mode]->Update(now, then);
    oled_func[oled_mode](now, then, false);
    then = now;    
}
