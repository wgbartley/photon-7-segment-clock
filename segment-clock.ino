#include "application.h"

#include "neopixel.h"
#define PIXEL_COUNT 58
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, D0, WS2812B);


#include "elapsedMillis.h"
elapsedMillis timerDHT22;
elapsedMillis timerEffect;
uint32_t intervalEffect;


#include "PietteTech_DHT.h"
void dht_wrapper();
PietteTech_DHT DHT(D2, DHT22, dht_wrapper);
bool dhtStarted = false;
String dhtError = "";
uint32_t dhtTimestamp = 0;
double dhtFahrenheit = 0;
double dhtHumidity = 0;
double dhtDewPoint = 0;

// Outside weather
float wxFahrenheit = 0;
float wxHumidity = 0;
uint32_t wxTimestamp = 0;
elapsedMillis wxTimer = 0;
uint32_t wxInterval = 3600000;
char wxStation[5] = "KCHA";


void randomColor();
void blackOut();
void displayDigit(uint8_t d, byte n);
void displayDigit(uint8_t d, byte n, uint8_t c[3]);
void rainbow(uint8_t wait);
uint32_t Wheel(byte WheelPos);
void doDHT22();
void doTime();
void displayHour();
void displayHour12();
void displayHour24();
void displayMinute();
void displayColon();

uint16_t weather_checked_i = 0;


#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
//#define bitSet(value, bit) ((value) |= (1UL << (bit)))
//#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
//#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

uint8_t color[3] = {0, 0, 64};

static const byte n0 = 0B01111110;
static const byte n1 = 0B01000010;
static const byte n2 = 0B00110111;
static const byte n3 = 0B01100111;
static const byte n4 = 0B01001011;
static const byte n5 = 0B01101101;
static const byte n6 = 0B01111101;
static const byte n7 = 0B01000110;
static const byte n8 = 0B01111111;
static const byte n9 = 0B01001111;

// EEPROM
// Address 0 = 117 if values have been saved
// Address 1 = 0/1 for -/+ of time zone offset
// Address 2 = Time zone offset (positive integer)
// Address 3 = 12/24 for hour format
// Address 4 = Effect mode
// Address 5 = Red
// Address 6 = Green
// Address 7 = Blue
// Address 8 = Rainbow delay
// Address 9 = Station code, 1st letter
// Address 10 = Station code, 2nd letter
// Address 11 = Station code, 3rd letter
// Address 12 = Station code, 4th letter
// Address 13 = Brightness

int8_t timeZone = 0;
bool time12Hour = false;
bool resetFlag = false;
elapsedMillis timerReset = 0;

// Effect modes
// 0 = no effect
// 1 = rainbow
// 2 = display local (indoor) environmentals
// 3 = display outdoor environmentals
// 4 = cycle modes (time, local envs, outdoor envs)
uint8_t EFFECT_MODE = 0;
uint8_t LAST_EFFECT_MODE = EFFECT_MODE;
uint8_t currEffect = EFFECT_MODE;
uint16_t RAINBOW_DELAY = 50;
uint8_t LAST_MINUTE = 0;
uint8_t BRIGHTNESS = 255;

SYSTEM_MODE(SEMI_AUTOMATIC);
bool has_booted = false;

void ledChangeHandler(uint8_t r, uint8_t g, uint8_t b) {
    if(EFFECT_MODE==2 || EFFECT_MODE==3)
        return;

    // Initial boot, control all teh LEDs
    if(!Particle.connected() && !has_booted) {
        for(uint8_t i=0; i<PIXEL_COUNT; i++)
            strip.setPixelColor(i, strip.Color(r, g, b));

        strip.show();

    // If we've booted at least once, only show the LED color
    } else {
        strip.setPixelColor(28, strip.Color(r, g, b));
        strip.setPixelColor(29, strip.Color(r, g, b));
    }
}

void setup() {
    strip.begin();
    strip.show();

    RGB.onChange(ledChangeHandler);

    while(!Particle.connected()) {
        Particle.connect();
        Particle.process();
        delay(1);
    }

    for(uint8_t i=0; i<PIXEL_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));

        if(i>0)
            strip.setPixelColor(i-1, strip.Color(0, 0, 0));

        strip.show();
        delay(10);
    }
    
    blackOut();

    Spark.variable("Fahrenheit", &dhtFahrenheit, DOUBLE);
    Spark.variable("Humidity", &dhtHumidity, DOUBLE);
    Spark.variable("DewPoint", &dhtDewPoint, DOUBLE);
    Spark.variable("dhtError", &dhtError, STRING);
    Spark.variable("dhtTS", &dhtTimestamp, INT);

    Spark.function("function", fnRouter);

    Spark.subscribe("hook-response/get_weather_gov", doWeather, MY_DEVICES);
    Spark.publish("get_weather_gov", wxStation);

    // See if this EEPROM has saved data
    if(EEPROM.read(0)==117) {
        // Set the time zone
        if(EEPROM.read(1)==0)
            timeZone = EEPROM.read(2)*-1;
        else
            timeZone = EEPROM.read(2);

        // Set the hour format
        if(EEPROM.read(3)==12)
            time12Hour = true;
        else
            time12Hour = false;

        EFFECT_MODE = EEPROM.read(4);
        LAST_EFFECT_MODE = EFFECT_MODE;

        color[0] = EEPROM.read(5);
        color[1] = EEPROM.read(6);
        color[2] = EEPROM.read(7);

        wxStation[0] = char(EEPROM.read(9));
        wxStation[1] = char(EEPROM.read(10));
        wxStation[2] = char(EEPROM.read(11));
        wxStation[3] = char(EEPROM.read(12));
        BRIGHTNESS = EEPROM.read(13);

        RAINBOW_DELAY = EEPROM.read(8);

    // If data has not been saved, "initialize" the EEPROM
    } else {
        // Initialize
        EEPROM.write(0, 117);
        // Time zone +/-
        EEPROM.write(1, 0);
        // Time zone
        EEPROM.write(2, 0);
        // Hour format
        EEPROM.write(3, 24);
        // Effect mode
        EEPROM.write(4, 0);
        // Red
        EEPROM.write(5, 0);
        // Green
        EEPROM.write(6, 255);
        // Blue
        EEPROM.write(7, 128);
        // Rainbow delay
        EEPROM.write(8, RAINBOW_DELAY);
        // Weather station
        EEPROM.write(9, int("K"));
        EEPROM.write(10, int("C"));
        EEPROM.write(11, int("H"));
        EEPROM.write(12, int("A"));
        EEPROM.write(13, 255);
    }

    Time.zone(timeZone);

    blackOut();
    strip.setBrightness(BRIGHTNESS);
    strip.show();

    has_booted = true;
}


void loop() {
    doEffectMode();
    doDHT22();
    checkWeather();

    if(timerReset>=500) {
        if(resetFlag) {
            System.reset();
            resetFlag = false;
        }

        timerReset = 0;
    }

    if(!Particle.connected())
        Particle.connect();

    Particle.process();
}


int fnRouter(String command) {
    command.trim();
    command.toUpperCase();

    // Set 12-hour format
    if(command.equals("SETHOURFORMAT12")) {
        time12Hour = true;
        EEPROM.write(3, 12);
        intervalEffect = 0;
        return 12;


    // Set 24-hour format
    } else if(command.equals("SETHOURFORMAT24")) {
        time12Hour = false;
        EEPROM.write(3, 24);
        intervalEffect = 0;
        return 24;


    // Get hour format
    } else if(command.equals("GETHOURFORMAT")) {
        if(time12Hour)
            return 12;
        else
            return 24;


    // Get time zone offset
    } else if(command.equals("GETTIMEZONE")) {
        return timeZone;


    // Set time zone offset
    } else if(command.substring(0, 12)=="SETTIMEZONE,") {
        timeZone = command.substring(12).toInt();
        Time.zone(timeZone);

        if(timeZone>-1) {
            EEPROM.write(1, 1);
            EEPROM.write(2, timeZone);
        } else {
            EEPROM.write(1, 0);
            EEPROM.write(2, timeZone * -1);
        }

        intervalEffect = 0;
        return timeZone;


    // Lazy way to reboot
    } else if(command.equals("REBOOT")) {
        resetFlag = true;
        timerReset = 0;
        return 1;


    // Set red
    } else if(command.substring(0, 7)=="SETRED,") {
        color[0] = command.substring(7).toInt();
        EEPROM.write(5, color[0]);
        intervalEffect = 0;

        return color[0];


    // Set green
    } else if(command.substring(0, 9)=="SETGREEN,") {
        color[1] = command.substring(9).toInt();
        EEPROM.write(6, color[1]);
        intervalEffect = 0;

        return color[1];


    // Set blue
    } else if(command.substring(0, 8)=="SETBLUE,") {
        color[2] = command.substring(8).toInt();
        EEPROM.write(7, color[2]);
        intervalEffect = 0;

        return color[2];


    // Set RGB
    } else if(command.substring(0, 7)=="SETRGB,") {
        color[0] = command.substring(7, 10).toInt();
        color[1] = command.substring(11, 14).toInt();
        color[2] = command.substring(15, 18).toInt();

        EEPROM.write(5, color[0]);
        EEPROM.write(6, color[1]);
        EEPROM.write(7, color[2]);

        intervalEffect = 0;

        return 1;


    // Random color
    } else if(command.equals("RANDOMCOLOR")) {
        randomColor();
        intervalEffect = 0;

        return 1;


    // Set effect mode
    } else if(command.substring(0, 10)=="SETEFFECT,") {
        EFFECT_MODE = command.substring(10).toInt();
        EEPROM.write(4, EFFECT_MODE);
        intervalEffect = 0;

        if(EFFECT_MODE==3)
            Spark.publish("get_weather_gov", wxStation);

        return EFFECT_MODE;


    // Get effect mode
    } else if(command.equals("GETEFFECTMODE")) {
        return EFFECT_MODE;


    // Get pixel color
    } else if(command.substring(0, 14)=="GETPIXELCOLOR,") {
        return strip.getPixelColor(command.substring(14).toInt());


    // Set rainbow effect delay
    } else if(command.substring(0, 16)=="SETRAINBOWDELAY,") {
        RAINBOW_DELAY = command.substring(16).toInt();
        intervalEffect = RAINBOW_DELAY;
        EEPROM.write(8, RAINBOW_DELAY);
        return RAINBOW_DELAY;

    // Get rainbow effect delay
    } else if(command.equals("GETRAINBOWDELAY")) {
        return RAINBOW_DELAY;

    // Get the value of red
    } else if(command.equals("GETRED")) {
        return color[0];

    // Get the value of green
    } else if(command.equals("GETGREEN")) {
        return color[1];

    // Get the value of blue
    } else if(command.equals("GETBLUE")) {
        return color[2];

    // Set the weather station
    } else if(command.substring(0, 13)=="SETWXSTATION,") {
        String station = command.substring(13);
        station.toUpperCase();
        station.toCharArray(wxStation, 5);

        EEPROM.write(9, int(wxStation[0]));
        EEPROM.write(10, int(wxStation[1]));
        EEPROM.write(11, int(wxStation[2]));
        EEPROM.write(12, int(wxStation[3]));

        return int(wxStation[0])+int(wxStation[1])+int(wxStation[2])+int(wxStation[3]);

    // Force weather update
    } else if(command.equals("UPDATEWX")) {
        Spark.publish("get_weather_gov", wxStation);
        return 1;

    // Brightness
    } else if(command.substring(0, 14)=="SETBRIGHTNESS,") {
        strip.setBrightness(command.substring(14).toInt());
        EEPROM.write(13, command.substring(14).toInt());
        return command.substring(14).toInt();
    }


    return -1;
}


void randomColor() {
    color[0] = random(32, 255);
    color[1] = random(32, 255);
    color[1] = random(32, 255);
}


void blackOut() {
    // Black it out
    for(uint8_t x=0; x<PIXEL_COUNT; x++)
        strip.setPixelColor(x, strip.Color(0, 0, 0));
}


void displayDigit(uint8_t d, byte n) {
    uint8_t x = 0;

    if(d>=2)
        x = 2;

    for(uint8_t i=0; i<7; i++) {
        if(bitRead(n, i)==1) {
            strip.setPixelColor((14*d)+i*2+x, strip.Color(color[0], color[1], color[2]));
            strip.setPixelColor((14*d)+i*2+1+x, strip.Color(color[0], color[1], color[2]));
        }
    }
}


void displayDigit(uint8_t d, byte n, uint8_t c[3]) {
    uint8_t x = 0;

    if(d>=2)
        x = 2;

    for(uint8_t i=0; i<7; i++) {
        if(bitRead(n, i)==1) {
            strip.setPixelColor((14*d)+i*2+x, strip.Color(c[0], c[1], c[2]));
            strip.setPixelColor((14*d)+i*2+1+x, strip.Color(c[0], c[1], c[2]));
        }
    }
}


void rainbow(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256; j++) {
        for(i=0; i<strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel((i+j) & 255));
        }

        strip.show();
        delay(wait);
    }
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    if(WheelPos < 85) {
        return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if(WheelPos < 170) {
        WheelPos -= 85;
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
        WheelPos -= 170;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
}


void doDHT22() {
    if(timerDHT22>2000) {
        if(!dhtStarted) {
            DHT.acquire();
            dhtStarted = true;
        }

        if(!DHT.acquiring()) {
            int dhtResult = DHT.getStatus();

            switch (dhtResult) {
                case DHTLIB_OK:
                    dhtError = "";
                    break;
                case DHTLIB_ERROR_CHECKSUM:
                    dhtError = "Checksum";
                    break;
                case DHTLIB_ERROR_ISR_TIMEOUT:
                    dhtError = "ISR timeout";
                    break;
                case DHTLIB_ERROR_RESPONSE_TIMEOUT:
                    dhtError = "Response timeout";
                    break;
                case DHTLIB_ERROR_DATA_TIMEOUT:
                    dhtError = "Data timeout";
                    break;
                case DHTLIB_ERROR_ACQUIRING:
                    dhtError = "Acquiring";
                    break;
                case DHTLIB_ERROR_DELTA:
                    dhtError = "Delta time to small";
                    break;
                case DHTLIB_ERROR_NOTSTARTED:
                    dhtError = "Not started";
                    break;
                default:
                    dhtError = "Unknown";
                    break;
            }

            if(dhtResult==DHTLIB_OK) {
                dhtTimestamp = Time.now();

                dhtHumidity = DHT.getHumidity();
                dhtFahrenheit = DHT.getFahrenheit();
                dhtDewPoint = DHT.getDewPoint();

                //String pub = "{\"h\":" + String((float)dhtHumidity) + ",\"f\":" + String((float)dhtFahrenheit) + ",\"d\":" + String((float)dhtDewPoint) + "}";
                //Spark.publish("environmentals", pub, 2);

                String pub = "h:"+String((float)dhtHumidity)+"|g,f:"+String((float)dhtFahrenheit)+"|g";
                Spark.publish("statsd", pub, 60);
            }

            dhtStarted = false;
        }

        timerDHT22 = 0;
    }
}


void doTime() {
    blackOut();

    displayHour();
    displayMinute();
    displayColon();

    strip.show();
}


void displayHour() {
    if(time12Hour)
        displayHour12();
    else
        displayHour24();
}


void displayHour12() {
    switch(Time.hourFormat12()) {
        case 0:
        case 12:
            displayDigit(0, n1);
            displayDigit(1, n2);
            break;
        case 1:
            displayDigit(0, n0);
            displayDigit(1, n1);
            break;
        case 2:
            displayDigit(0, n0);
            displayDigit(1, n2);
            break;
        case 3:
            displayDigit(0, n0);
            displayDigit(1, n3);
            break;
        case 4:
            displayDigit(0, n0);
            displayDigit(1, n4);
            break;
        case 5:
            displayDigit(0, n0);
            displayDigit(1, n5);
            break;
        case 6:
            displayDigit(0, n0);
            displayDigit(1, n6);
            break;
        case 7:
            displayDigit(0, n0);
            displayDigit(1, n7);
        case 8:
            displayDigit(0, n0);
            displayDigit(1, n8);
            break;
        case 9:
            displayDigit(0, n0);
            displayDigit(1, n9);
            break;
        case 10:
            displayDigit(0, n1);
            displayDigit(1, n0);
            break;
        case 11:
            displayDigit(0, n1);
            displayDigit(1, n1);
            break;
    }
}


void displayHour24() {
    // Display hour - 1st digit
    switch(Time.hour()/10) {
        case 0:
            displayDigit(0, n0);
            break;
        case 1:
            displayDigit(0, n1);
            break;
        case 2:
            displayDigit(0, n2);
            break;
    }


    // Display hour - 2nd digit
    switch(Time.hour()%10) {
        case 0:
            displayDigit(1, n0);
            break;
        case 1:
            displayDigit(1, n1);
            break;
        case 2:
            displayDigit(1, n2);
            break;
        case 3:
            displayDigit(1, n3);
            break;
        case 4:
            displayDigit(1, n4);
            break;
        case 5:
            displayDigit(1, n5);
            break;
        case 6:
            displayDigit(1, n6);
            break;
        case 7:
            displayDigit(1, n7);
            break;
        case 8:
            displayDigit(1, n8);
            break;
        case 9:
            displayDigit(1, n9);
            break;
    }
}


void displayMinute() {
    // Display minute - 1st digit
    switch(Time.minute()/10) {
        case 0:
            displayDigit(2, n0);
            break;
        case 1:
            displayDigit(2, n1);
            break;
        case 2:
            displayDigit(2, n2);
            break;
        case 3:
            displayDigit(2, n3);
            break;
        case 4:
            displayDigit(2, n4);
            break;
        case 5:
            displayDigit(2, n5);
            break;
    }


    // Display minute - 2nd digit
    switch(Time.minute()%10) {
        case 0:
            displayDigit(3, n0);
            break;
        case 1:
            displayDigit(3, n1);
            break;
        case 2:
            displayDigit(3, n2);
            break;
        case 3:
            displayDigit(3, n3);
            break;
        case 4:
            displayDigit(3, n4);
            break;
        case 5:
            displayDigit(3, n5);
            break;
        case 6:
            displayDigit(3, n6);
            break;
        case 7:
            displayDigit(3, n7);
            break;
        case 8:
            displayDigit(3, n8);
            break;
        case 9:
            displayDigit(3, n9);
            break;
    }
}


void displayColon() {
    //strip.setPixelColor(28, strip.Color(color[0], color[1], color[2]));
    //strip.setPixelColor(29, strip.Color(color[0], color[1], color[2]));
}


void dht_wrapper() {
    DHT.isrCallback();
}


void doEffectMode() {
    if(EFFECT_MODE!=LAST_EFFECT_MODE) {
        blackOut();
        LAST_EFFECT_MODE = EFFECT_MODE;
    }

    if(timerEffect>=intervalEffect) {
        timerEffect = 0;

        // if(Time.hour()==13 && Time.minute()==37)
        //     EFFECT_MODE = 77;

        switch(EFFECT_MODE) {
            case 1: // Rainbow
                doEffectRainbow();
                intervalEffect = RAINBOW_DELAY;
                break;

            case 2: // Temp/humidity
                doEffectEnvironmentals(true);
                intervalEffect = 2000;
                break;

            case 3: // Internet temp/humidity
                doEffectEnvironmentals(false);
                intervalEffect = 1000;
                break;

            case 4: // Cycle
                //doTime();
                //intervalEffect = 5000;

            case 77: // L33t
                doLeet();
                break;

            case 0: // Time
            default:
                doTime();
                intervalEffect = 1000;
        }
    }
}


void doEffectRainbow() {
    uint16_t i, j;

    if(Time.minute()!=LAST_MINUTE) {
        blackOut();
        LAST_MINUTE = Time.minute();
    }

    displayHour();
    displayMinute();
    displayColon();

    for(j=0; j<256; j++) {
        if(EFFECT_MODE!=1) break;

        for(i=0; i<strip.numPixels(); i++) {
            if(i==28 || i==29)
                continue;

            if(strip.getPixelColor(i)>0)
                strip.setPixelColor(i, Wheel((i+j) & 255));
        }

        strip.show();
        delay(RAINBOW_DELAY);
        Spark.process();
    }
}


void doLeet() {
    displayHour();
    displayMinute();
    displayColon();

    for(uint8_t i=0; i<strip.numPixels(); i++)
        if(strip.getPixelColor(i)>0)
            strip.setPixelColor(i, strip.Color(0, 255, 0));

    strip.show();
    Spark.process();
}


void doEffectEnvironmentals(bool local) {
    blackOut();

    int _f;
    int _h;

    if(local) {
        _f = (int) dhtFahrenheit;
        _h = (int) dhtHumidity;

        strip.setPixelColor(28, strip.Color(0, 255, 0));
        strip.setPixelColor(29, strip.Color(0, 0, 0));
    } else {
        _f = (int) wxFahrenheit;
        _h = (int) wxHumidity;

        strip.setPixelColor(28, strip.Color(0, 0, 0));
        strip.setPixelColor(29, strip.Color(0, 255, 0));
    }

    uint8_t c[3] = {255, 128, 0};
    // Display temp - first digit
    switch(_f/10) {
        case 0:
            displayDigit(0, n0, c);
            break;
        case 1:
            displayDigit(0, n1, c);
            break;
        case 2:
            displayDigit(0, n2, c);
            break;
        case 3:
            displayDigit(0, n3, c);
            break;
        case 4:
            displayDigit(0, n4, c);
            break;
        case 5:
            displayDigit(0, n5, c);
            break;
        case 6:
            displayDigit(0, n6, c);
            break;
        case 7:
            displayDigit(0, n7, c);
            break;
        case 8:
            displayDigit(0, n8, c);
            break;
        case 9:
            displayDigit(0, n9, c);
            break;
    }


    // Display temperature - 2nd digit
    switch(_f%10) {
        case 0:
            displayDigit(1, n0, c);
            break;
        case 1:
            displayDigit(1, n1, c);
            break;
        case 2:
            displayDigit(1, n2, c);
            break;
        case 3:
            displayDigit(1, n3, c);
            break;
        case 4:
            displayDigit(1, n4, c);
            break;
        case 5:
            displayDigit(1, n5, c);
            break;
        case 6:
            displayDigit(1, n6, c);
            break;
        case 7:
            displayDigit(1, n7, c);
            break;
        case 8:
            displayDigit(1, n8, c);
            break;
        case 9:
            displayDigit(1, n9, c);
            break;
    }


    c[0] = 0;
    c[1] = 128;
    c[2] = 255;
    // Display humidity - 1st digit
    switch(_h/10) {
        case 0:
            displayDigit(2, n0, c);
            break;
        case 1:
            displayDigit(2, n1, c);
            break;
        case 2:
            displayDigit(2, n2, c);
            break;
        case 3:
            displayDigit(2, n3, c);
            break;
        case 4:
            displayDigit(2, n4, c);
            break;
        case 5:
            displayDigit(2, n5, c);
            break;
        case 6:
            displayDigit(2, n6, c);
            break;
        case 7:
            displayDigit(2, n7, c);
            break;
        case 8:
            displayDigit(2, n8, c);
            break;
        case 9:
            displayDigit(2, n9, c);
            break;
    }


    // Display humidity - 2nd digit
    switch(_h%10) {
        case 0:
            displayDigit(3, n0, c);
            break;
        case 1:
            displayDigit(3, n1, c);
            break;
        case 2:
            displayDigit(3, n2, c);
            break;
        case 3:
            displayDigit(3, n3, c);
            break;
        case 4:
            displayDigit(3, n4, c);
            break;
        case 5:
            displayDigit(3, n5, c);
            break;
        case 6:
            displayDigit(3, n6, c);
            break;
        case 7:
            displayDigit(3, n7, c);
            break;
        case 8:
            displayDigit(3, n8, c);
            break;
        case 9:
            displayDigit(3, n9, c);
            break;
    }

    strip.show();
}


void checkWeather() {
    if(wxTimer>=wxInterval) {
        Spark.publish("get_weather_gov", wxStation);

        wxTimer = 0;
    }
}


void doWeather(const char *name, const char *data) {
    String str = String(data);
    //String weatherStr = tryExtractString(str, "<weather>", "</weather>");

    String temp_f = tryExtractString(str, "<temp_f>", "</temp_f>");
    String humidity = tryExtractString(str, "<relative_humidity>", "</relative_humidity>");

    if(temp_f!=NULL) {
        wxFahrenheit = temp_f.toFloat();
        wxTimestamp = Time.now();
    }

    if(humidity!=NULL)
        wxHumidity = humidity.toFloat();

    if(EFFECT_MODE==3)
        doEffectEnvironmentals(false);
}


String tryExtractString(String str, const char* start, const char* end) {
    if (str == NULL) {
        return NULL;
    }

    int idx = str.indexOf(start);
    if (idx < 0) {
        return NULL;
    }

    int endIdx = str.indexOf(end);
    if (endIdx < 0) {
        return NULL;
    }

    return str.substring(idx + strlen(start), endIdx);
}