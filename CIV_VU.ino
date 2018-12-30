#include <Adafruit_NeoPixel.h>

#define N_PIXELS  30  // Number of pixels in strand
#define LED_PIN    6  // NeoPixel LED strand is connected to this pin
#define TOP       (N_PIXELS + 1) // Allow dot to go slightly off scale
#define PEAK_FALL 3  // Rate of peak falling dot

#define REMOTE_ADDR 0x60 // Default for IC-910H
#define LOCAL_ADDR 0xE0 // Our Address

byte peak = 0;      // Used for falling dot
byte dotCount  = 0;      // Frame counter for delaying dot-falling speed

uint8_t sLevel = 0; //Decimal signal level 0-255
uint8_t retries = 0; //Connection loss counter

unsigned long previousMillis = 0; //Timer stuffs
const long timeout = 100; // Timeout for response

unsigned char response[9]; // Buffer for received data

Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
    // Initialize hardware
    strip.begin();
    Serial.begin(19200);

    // Initialize data
    memset(response, 0, sizeof(response));
}

void loop()
{
    uint8_t tempLevel = sMeter();

    if(tempLevel == 0) //Not the best check but...
    {                  //It's highly likely an actual 0 from S means lost connection
        retries++;
    }
    else
    {   //Got something, carry on.
        sLevel = tempLevel;
        retries = 0;
    }
    
    // This counts the number of times we went for a loop without anything
    // Die and try again
    if(retries >= 25)
    {
      sLevel = 0;
      retries = 0;
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

uint8_t sMeter()
{
    uint8_t meter = 0;
    memset(response, 0, sizeof(response)); // Clear the response buffer

    Serial.write(0xFE);
    Serial.write(0xFE);
    Serial.write(REMOTE_ADDR);
    Serial.write(LOCAL_ADDR);
    Serial.write(0x15);
    Serial.write(0x02);
    Serial.write(0xFD);
    Serial.flush();
    delay(1); // Experience tells me to give a little time to clear the buffer.

    previousMillis = millis();
    while(!Serial.available()) // Waiting
    {
        if(millis() - previousMillis >= timeout) // Too long, try again next time.
         {
            break;
         }
     }

     int idx = 0;
     while(Serial.available()) // We have something!
     {
         unsigned char c = Serial.read();
         response[idx] = c;
         idx++;
         delay(1); // Help keep the buffer full.
     }
        
    if(idx >= 8) // Got some data, let's take a look.
    {
        if(response[0] == 0xFE && response[1] == 0xFE) // Looks like a valid preamble, go on.
        {
            if(response[2] == LOCAL_ADDR && response[3] == REMOTE_ADDR) // From our rig to us.
            {
                if(response[4] == 0x15 && response[5] == 0x02) // Correct command and subcommand.
                {
                    if(response[7] == 0xFD) // Check position of end of message to see the size of data.
                    {
                        meter = bcdToDec(response[6]);
                    }
                    else if(response[8] == 0xFD)
                    {
                        meter = bcdToDec(response[6]) * 100; //MSB
                        meter += bcdToDec(response[7]);
                    }
                }
            }
        }
    }
    return meter;
}

uint8_t bcdToDec(uint8_t val) // Does what it says
{
    return( (val/16*10) + (val%16) );
}

