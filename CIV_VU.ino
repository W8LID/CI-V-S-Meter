#include <Adafruit_NeoPixel.h>

#define N_PIXELS  30  // Number of pixels in strand
#define LED_PIN    6  // NeoPixel LED strand is connected to this pin
#define TOP       (N_PIXELS + 2) // Allow dot to go slightly off scale
#define PEAK_FALL 15  // Rate of peak falling dot

#define REMOTE_ADDR 0x60 // Default for IC-910H
#define LOCAL_ADDR 0xE0 // Our Address

byte peak = 0;      // Used for falling dot
byte dotCount  = 0;      // Frame counter for delaying dot-falling speed

int sLevel = 0; //Decimal signal level 0-255
int retries = 0; //Connection loss counter

unsigned long previousMillis = 0; //Timer stuffs
const long interval = 30; // Keep updates to 30mS or more because that's what the rig likes YMMV

 // Our data, this is ugly  and will disappear soon
 // I don't even like the names, what was I thinking?
byte firstS = 0, secondS = 0, thirdS = 0;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
    // Initialize hardware
    strip.begin();
    Serial.begin(19200);
}

void loop()
{
    int idx = 0;

    // Request data, intervals shorter than 30mS seem to cause problems.
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) 
    {
        previousMillis = currentMillis;
        readSMeter();
    }

    while(Serial.available()) // We have something!
    {
        // We make a lot of assumptions here and this will all be refactored soon.
        // We have no idea if we're getting our response or something else and 
        // assume we're the only traffic.
        char c = Serial.read();
        if(idx == 6)
        {
            firstS = c;
        }
        if(idx == 7)
        {
            secondS = c;
        }
        
        if(idx == 8)
        {
            thirdS = c;
        }

        idx++;
        delay(1); // Help keep the buffer full.
        retries = 0; // We got something, reset the counter but we should do this somewhere else
    }

    // This counts the number of times we went for a loop without anything 
    // being received by the UART, prevents glitches.
    if(retries >= 1500)
    {
      firstS = 0,secondS = 0,thirdS = 0;
      sLevel = 0;
      retries = 0;
    }

    // Look for end of message, its the only thing we check right now.
    if(secondS == 0xFD)
    {
        sLevel = bcdToDec(firstS);
    }
    else if(thirdS == 0xFD)
    {
        sLevel = bcdToDec(secondS);
        sLevel += bcdToDec(firstS) * 100;
    }

    // How high are we?
    int height = TOP * (sLevel / 255.0);
    
    if(height < 0L)// Clip output
    {
        height = 0;
    }
    else if(height > TOP)
    {
        height = TOP;
    }
    
    if(height > peak)
    {
        peak   = height; // Keep 'peak' dot at top
    }

    uint8_t  i;

    // Color pixels based on rainbow gradient
    for(i = 0; i < N_PIXELS; i++)
    {
        if(i >= height)
        {
            strip.setPixelColor(i,   0,   0, 0);
        }
        else
        {
            strip.setPixelColor(i,Wheel(map(i,0,strip.numPixels()-1,30,150)));
        }
        
    }
    
    // Draw peak dot
    if(peak > 0 && peak <= N_PIXELS-1)
    {
        strip.setPixelColor(peak,Wheel(map(peak,0,strip.numPixels()-1,30,150)));
    }
    
    strip.show(); // Update strip
    
    // Every few frames, make the peak pixel drop by 1:
    if(++dotCount >= PEAK_FALL)
    { //fall rate
        
        if(peak > 0)
        {
            peak--;
        }
        dotCount = 0;
    }

    // Increment the retry counter
    retries++;
}

// Input a value 0 to 255 to get a color value.
// We borrowed this and modified to go from green
// to red so it's more like a VU meter.
uint32_t Wheel(byte WheelPos)
{
    if(WheelPos < 128)
    {
        return strip.Color(WheelPos, 128 - WheelPos, 0);
    }
    else if(WheelPos < 192)
    {
        WheelPos -= 128;
        return strip.Color(128 - WheelPos, 0, 0);
    }
    else
    {
        WheelPos -= 128;
        return strip.Color(128 - WheelPos, WheelPos, 0);
    }
}

void readSMeter() //CI-V to read the S meter
{
    Serial.write(0xFE);
    Serial.write(0xFE);
    Serial.write(REMOTE_ADDR);
    Serial.write(LOCAL_ADDR);
    Serial.write(0x15);
    Serial.write(0x02);
    Serial.write(0xFD);
    Serial.flush();
    delay(1); // Experience tells me to give a little time to clear the buffer.
}

byte bcdToDec(byte val) // Does what it says
{
    return( (val/16*10) + (val%16) );
}

