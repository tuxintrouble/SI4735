/*
  SS4735 SSB Test. Under constuction...
  Arduino Library example with LCD 20x4 I2C.
  Rotary Encoder: This sketch uses the Rotary Encoder Class implementation from Ben Buxton.
  The source code is included together with this sketch.

  By Ricardo Lima Caratti, Nov 2019.
*/

#include <LiquidCrystal_I2C.h>
#include <SI4735.h>
#include "patch_content.h"
#include "Rotary.h"

#define AM_FUNCTION 1
// OLED Diaplay constants
#define I2C_ADDRESS 0x3C
#define RST_PIN -1 // Define proper RST_PIN if required.

#define RESET_PIN 12

// Enconder PINs
#define ENCODER_PIN_A 3
#define ENCODER_PIN_B 2

// Buttons controllers
#define LSB_USB_AM_SWITCH 4
#define BANDWIDTH_BUTTON 5 // Next Band
#define BAND_BUTTON_UP 8   // Band Up
#define BAND_BUTTON_DOWN 9 // Band Down
#define VOL_UP 6           // Volume Up
#define VOL_DOWN 7         // Volume Down
#define BFO_SWITCH 10      // Switch Enconder to control BFO

#define MIN_ELAPSED_TIME 100

#define AM_MODE 0
#define LSB_MODE 1
#define USB_MODE 2

const int size_content_full = sizeof ssb_patch_content_full;

long elapsedButton = millis();

// Encoder control variables
volatile int encoderCount = 0;

// Some variables to check the SI4735 status
unsigned currentFrequency;
unsigned previousFrequency;

byte bandwidthIdx = 0;
char *bandwitdth[] = {"6", "4", "3", "2", "1", "1.8", "2.5"};

typedef struct
{
  unsigned minimumFreq;
  unsigned maximumFreq;
  unsigned currentFreq;
  unsigned currentStep;
  byte currentMode;
  int currentBFO;
} Band;

Band band[] = {
    {3500, 4000, 3750, 1, LSB_MODE, 0},
    {7000, 7300, 7100, 1, LSB_MODE, 0},
    {14000, 14400, 14200, 1, USB_MODE, 0},
    {18000, 19000, 18100, 1, USB_MODE, 0},
    {2100, 21400, 21200, 1, USB_MODE, 0},
    {27000, 27500, 27220, 1, USB_MODE, 0},
    {28000, 28500, 28400, 1, USB_MODE, 0}};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int currentFreqIdx = 1; // 40M

byte rssi = 0;
byte stereo = 1;
byte volume = 0;

int currentBFO = 0;
int previousBFO = 0;
bool bfoOn = false;

byte currentMode = AM_MODE; // AM, LSB or USB

// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

LiquidCrystal_I2C display(0x27, 20, 4); // please check the address of your I2C device

SI4735 si4735;

void setup()
{

  Serial.begin(9600);
  // Encoder pins
  pinMode(LSB_USB_AM_SWITCH, INPUT);
  pinMode(ENCODER_PIN_A, INPUT);
  pinMode(ENCODER_PIN_B, INPUT);
  pinMode(BANDWIDTH_BUTTON, INPUT);
  pinMode(BAND_BUTTON_UP, INPUT);
  pinMode(BAND_BUTTON_DOWN, INPUT);
  pinMode(VOL_UP, INPUT);
  pinMode(VOL_DOWN, INPUT);
  pinMode(BFO_SWITCH, INPUT);

  display.init();

  delay(500);

  // Splash - Change it for your introduction text.
  display.setCursor(6, 0);
  display.print("Si4735");
  display.setCursor(2, 1);
  display.print("Arduino Library");
  delay(500);
  display.setCursor(4, 3);
  display.print("SSB Test");
  display.setCursor(4, 4);
  display.print("By PU2CLR");
  delay(3000);
  display.clear();
  // end Splash

  // Encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

  si4735.setup(RESET_PIN, 15);

  delay(500);
  si4735.setup(RESET_PIN, 0);

  delay(500);
  display.clear();

  si4735.setup(RESET_PIN, 1);
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSsbConfig(1, 0, 0, 1, 0, 1);
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentMode);

  currentFrequency = previousFrequency = si4735.getFrequency();
  si4735.setVolume(60);

  showStatus();
}

// Use Rotary.h and  Rotary.cpp implementation to process encoder via interrupt
void rotaryEncoder()
{ // rotary encoder events
  uint8_t encoderStatus = encoder.process();
  if (encoderStatus)
  {
    if (encoderStatus == DIR_CW)
    {
      encoderCount = 1;
    }
    else
    {
      encoderCount = -1;
    }
  }
}

// Show current frequency
void showStatus()
{
  String unit, freqDisplay;
  String bandMode;

  if (currentMode == AM_MODE)
    bandMode = String("AM ");
  else if (currentMode == LSB_MODE)
    bandMode = String("LSB");
  else
    bandMode = String("USB");

  freqDisplay = String(currentFrequency);

  display.setCursor(0, 0);
  display.print(String(bandMode));

  display.setCursor(16, 0);
  display.print(unit);

  display.setCursor(5, 0);
  display.print("        ");
  display.setCursor(5, 0);
  display.print(freqDisplay);

  display.setCursor(0, 2);
  display.print("            ");
  display.setCursor(0, 2);
  display.print("BW: ");
  display.print(String(bandwitdth[bandwidthIdx]));
  display.print(" KHz");
}

/* *******************************
   Shows RSSI status
*/
void showRSSI()
{
  display.setCursor(12, 4);
  display.print("S:");
  display.setCursor(14, 4);
  display.print("      ");
  display.setCursor(14, 4);
  display.print(rssi);
  display.print(" dBuV");
}

void showBFO()
{
  display.setCursor(0, 3);
  display.print("          ");
  display.setCursor(0, 3);
  display.print("BFO: ");
  display.print(currentBFO);
  display.print("Hz");
}

void showVolume() {
  display.setCursor(0, 4);
  display.print("      ");
  display.setCursor(0, 4);
  display.print("V:");
  display.print(currentBFO);
  display.print("Hz");
}

void bandUp()
{

  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if (currentFreqIdx < lastBand)
  {
    currentFreqIdx++;
  }
  else
  {
    currentFreqIdx = 0;
  }

  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  if (band[currentFreqIdx].currentMode == AM_MODE)
  {
    si4735.setAM(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep);
  }
  else
  {
    si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentMode);
    currentBFO = band[currentFreqIdx].currentBFO;
    si4735.setSsbBfo(currentBFO);
  }
}

void bandDown()
{
  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if (currentFreqIdx > 0)
  {
    currentFreqIdx--;
  }
  else
  {
    currentFreqIdx = lastBand;
  }

  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  if (band[currentFreqIdx].currentMode == AM_MODE)
  {
    si4735.setAM(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep);
  }
  else
  {
    si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentMode);
    currentBFO = band[currentFreqIdx].currentBFO;
    si4735.setSsbBfo(currentBFO);
  }
}

/*
   Patches to the SI4735 are transferred to the internal RAM of the device.
   All content of the patch will get lost when you switch to another mode like AM or FM.
   So, the content of the patch has to be transferred again to the device each time you select
   SSB mode.
*/
void loadSSB()
{
  si4735.queryLibraryId(); // Is it really necessary here? I will check it.
  si4735.patchPowerUp();
  si4735.downloadPatch(ssb_patch_content_full, size_content_full);
  si4735.setSsbConfig(2, 1, 0, 1, 0, 1);
  // si4735.setSSB( band[currentFreqIdx].currentMode);
  showStatus();
}

/*
   Main
*/
void loop()
{

  // Check if the encoder has moved.
  if (encoderCount != 0)
  {
    if (bfoOn)
    {
      currentBFO = (encoderCount == 1) ? (currentBFO + 10) : (currentBFO - 10);
      si4735.setSsbBfo(currentBFO);
    }
    else
    {
      if (encoderCount == 1)
        si4735.frequencyUp();
      else
        si4735.frequencyDown();
      encoderCount = 0;
    }
  }
  // Check button commands
  if (digitalRead(BANDWIDTH_BUTTON) | digitalRead(BAND_BUTTON_UP) | digitalRead(BAND_BUTTON_DOWN) | digitalRead(VOL_UP) | digitalRead(VOL_DOWN) | digitalRead(LSB_USB_AM_SWITCH) | digitalRead(BFO_SWITCH))
  {
    if ((millis() - elapsedButton) > MIN_ELAPSED_TIME)
    {
      // check if some button is pressed
      if (digitalRead(BANDWIDTH_BUTTON) == HIGH)
      {
        bandwidthIdx++;
        if (bandwidthIdx > 6)
          bandwidthIdx = 0;

        si4735.setBandwidth(bandwidthIdx, 0);
        showStatus();
      }
      else if (digitalRead(BAND_BUTTON_UP) == HIGH)
        bandUp();
      else if (digitalRead(BAND_BUTTON_DOWN) == HIGH)
        bandDown();
      else if (digitalRead(VOL_UP) == HIGH)
        si4735.volumeUp();
      else if (digitalRead(VOL_DOWN) == HIGH)
        si4735.volumeDown();
      else if (digitalRead(LSB_USB_AM_SWITCH) == HIGH)
      {
        if (currentMode == AM_MODE)
        {
          loadSSB();
          currentMode = band[currentFreqIdx].currentMode = LSB_MODE;
          si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, currentMode);
        }
        else if (currentMode == LSB_MODE)
        {
          currentMode = band[currentFreqIdx].currentMode = USB_MODE;
          band[currentFreqIdx].currentBFO = currentBFO;
          si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, currentMode);
        }
        else if (currentMode == USB_MODE)
        {
          band[currentFreqIdx].currentBFO = currentBFO;
          currentMode = band[currentFreqIdx].currentMode = AM_MODE;
          si4735.setAM(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep);
        }
      }
      else if (digitalRead(BFO_SWITCH) == HIGH)
      {
        bfoOn = !bfoOn;
      }

      elapsedButton = millis();
    }

    // Show the current frequency only if it has changed
    currentFrequency = si4735.getFrequency();
    if (currentFrequency != previousFrequency)
    {
      previousFrequency = currentFrequency;
      showStatus();
    }

    // Show RSSI status only if this condition has changed
    if (rssi != si4735.getCurrentRSSI())
    {
      rssi = si4735.getCurrentRSSI();
      showRSSI();
    }

    if (currentBFO != previousBFO)
    {
      previousBFO = currentBFO;
      showBFO();
    }
    delay(5);
  }
}