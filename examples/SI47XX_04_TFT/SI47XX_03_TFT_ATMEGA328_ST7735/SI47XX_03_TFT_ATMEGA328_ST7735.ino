/*
  This sketch uses an Arduino Pro Mini, 3.3V (8MZ) with a SPI TFT ST7735 1.8"

  The  purpose  of  this  example  is  to  demonstrate a prototype  receiver based  on  the  SI4735  and  the 
  "PU2CLR SI4735 Arduino Library" working with the TFT ST7735 display. It is not the purpose of this prototype 
  to provide you a beautiful interface. To be honest, I think you can do it better than me. 

  It is  a  complete  radio  capable  to  tune  LW,  MW,  SW  on  AM  and  SSB  mode  and  also  receive  the
  regular  comercial  stations. If  you  are  using  the  same  circuit  used  on  examples with OLED and LCD,
  you have to change some buttons wire up. This TFT device takes five pins from Arduino.
  For this reason, it is necessary change the pins of some buttons.
  Fortunately, you can use the ATmega328 analog pins as digital pins.

  The libraries Adafruit_GFX and Adafruit_ST7735 take a lot of memory space from Arduino. 
  You have few space to improve your prototype with standard Arduino Pro Mini.
  However, you can use some approaches:  
  1. Shrink or remove the boot loader from Arduino Pro Mini;
  2. The Arduino Nano e Uno has smaller bootloader than the Arduino Pro Mini
  3. Port this sketch to a bigger board like Arduino Mega or DUE. 


  Features:   AM; SSB; LW/MW/SW; two super band (from 150Khz to 30 MHz); external mute circuit control; Seek (Automatic tuning)
              AGC; Attenuation gain control; SSB filter; CW; AM filter; 1, 5, 10, 50 and 500KHz step on AM and 10Hhz sep on SSB

  Wire up on Arduino UNO, Pro mini
  | Device name               | Device Pin / Description      |  Arduino Pin  |
  | ----------------          | ----------------------------- | ------------  |
  | Display TFT               |                               |               |
  |                           | RST (RESET)                   |      8        |
  |                           | RS or DC                      |      9        |
  |                           | CS or SS                      |     10        |
  |                           | SDI                           |     11        |
  |                           | CLK                           |     13        |
  |                           | BL                            |    +VCC       |  
  |     Si4735                |                               |               |
  |                           | RESET (pin 15)                |     12        |
  |                           | SDIO (pin 18)                 |     A4        |
  |                           | SCLK (pin 17)                 |     A5        |
  |     Buttons               |                               |               |
  |                           | (*)Switch MODE (AM/LSB/AM)    |      4        |
  |                           | (*)Banddwith                  |      5        |
  |                           | (*)BAND                       |      6        |
  |                           | SEEK                          |      7        |
  |                           | (*)AGC/Attenuation            |     14 / A0   |
  |                           | (*)STEP                       |     15 / A1   | 
  |                           | VFO/VFO Switch                |     16 / A2   |
  |    Encoder                |                               |               |
  |                           | A                             |       2       |
  |                           | B                             |       3       |

  (*) You have to press the push button and after, rotate the encoder to select the parameter
      After you activate a command by pressing a push button, it will keep active for 2,5 seconds 

  Prototype documentation: https://pu2clr.github.io/SI4735/
  PU2CLR Si47XX API documentation: https://pu2clr.github.io/SI4735/extras/apidoc/html/

  By PU2CLR, Ricardo,  Feb  2020.
*/

#include <SI4735.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

#include <SPI.h>
#include "Rotary.h"

// Test it with patch_init.h or patch_full.h. Do not try load both.
#include "patch_init.h" // SSB patch for whole SSBRX initialization string

const uint16_t size_content = sizeof ssb_patch_content; // see ssb_patch_content in patch_full.h or patch_init.h

// TFT MICROYUM or ILI9225 based device pin setup
#define TFT_RST 8
#define TFT_DC 9
#define TFT_CS 10  // SS
#define TFT_SDI 11 // MOSI
#define TFT_CLK 13 // SCK
#define TFT_LED 0  // 0 if wired to +3.3V directly
#define TFT_BRIGHTNESS 200


#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

#define RESET_PIN 12

// Enconder PINs
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3

// Buttons controllers
#define MODE_SWITCH 4         // Switch MODE (Am/LSB/USB)
#define BANDWIDTH_BUTTON 5    // Used to select the banddwith. Values: 1.2, 2.2, 3.0, 4.0, 0.5, 1.0 KHz
#define BAND_BUTTON 6         // Band switch button
#define SEEK_BUTTON 7         // Previous band
#define AGC_SWITCH 14         // Pin A0 - Switch AGC ON/OF
#define STEP_SWITCH 15        // Pin A1 - Used to select the increment or decrement frequency step (1, 5 or 10 KHz)
#define BFO_SWITCH 16         // Pin A3 - Used to select the enconder control (BFO or VFO)
#define AUDIO_MUTE 1          // External AUDIO MUTE circuit control

#define MIN_ELAPSED_TIME 200
#define MIN_ELAPSED_RSSI_TIME 150
#define ELAPSED_COMMAND 2500  // time to turn off the last command controlled by encoder
#define DEFAULT_VOLUME 50     // change it for your favorite sound volume

#define FM 0
#define LSB 1
#define USB 2
#define AM 3
#define LW 4

#define SSB 1
#define CLEAR_BUFFER(x)  (x[0] = '\0');

bool bfoOn = false;
bool ssbLoaded = false;
bool fmStereo = true;

// AGC and attenuation control
int8_t agcIdx = 0;
uint8_t disableAgc = 0;
uint8_t agcNdx = 0;

bool cmdBand = false;
bool cmdBfo = false;
bool cmdVolume = false;
bool cmdAgc = false;
bool cmdBandwidth = false;
bool cmdStep = false;
bool cmdMode = false;

int currentBFO = 0;
uint8_t seekDirection = 1;

long elapsedRSSI = millis();
long elapsedButton = millis();
long elapsedCommand = millis();

// Encoder control variables
volatile int encoderCount = 0;

// Some variables to check the SI4735 status
uint16_t currentFrequency;
uint16_t previousFrequency = 0;

uint8_t currentBFOStep = 10;

uint8_t bwIdxSSB = 2;
const char * bandwitdthSSB[] = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};

uint8_t bwIdxAM = 1;
const char * bandwitdthAM[] = {"6", "4", "3", "2", "1", "1.8", "2.5"};

const char * bandModeDesc[] = {"   ", "LSB", "USB", "AM "};
uint8_t currentMode = FM;

uint16_t currentStep = 1;

char bufferDisplay[20]; // Useful to handle string
char bufferFreq[15];
char bufferBFO[15];
char bufferStepVFO[15];
char bufferBW[15];
char bufferAGC[15];
char bufferBand[15];
char bufferStereo[15];
char bufferUnt[5];

/*
   Band data structure
*/
typedef struct
{
  const char *bandName; // Band description
  uint8_t bandType;     // Band type (FM, MW or SW)
  uint16_t minimumFreq; // Minimum frequency of the band
  uint16_t maximumFreq; // maximum frequency of the band
  uint16_t currentFreq; // Default frequency or current frequency
  uint16_t currentStep; // Defeult step (increment and decrement)
} Band;

/*
   Band table
   Actually, except FM (VHF), the other bands cover the entire LW / MW and SW spectrum.
   Only the default frequency and step is changed. You can change this setup. 
*/
Band band[] = {
    {"FM ", FM_BAND_TYPE, 6400, 10800, 10390, 10},
    {"MW ", MW_BAND_TYPE, 150, 1720, 810, 10},
    {"SW1", SW_BAND_TYPE, 150, 30000, 7100, 1}, // Here and below: 150KHz to 30MHz
    {"SW2", SW_BAND_TYPE, 150, 30000, 9600, 5},
    {"SW3", SW_BAND_TYPE, 150, 30000, 11940, 5},
    {"SW4", SW_BAND_TYPE, 150, 30000, 13600, 5},
    {"SW5", SW_BAND_TYPE, 150, 30000, 14200, 1},
    {"SW5", SW_BAND_TYPE, 150, 30000, 15300, 5},
    {"SW6", SW_BAND_TYPE, 150, 30000, 17600, 5},
    {"SW7", SW_BAND_TYPE, 150, 30000, 21100, 1},
    {"SW8", SW_BAND_TYPE, 150, 30000, 22525, 5},
    {"SW9", SW_BAND_TYPE, 150, 30000, 28400, 1}};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int bandIdx = 0;


int tabStep[] = {1, 5, 10, 50, 100, 500, 1000};
const int lastStep = (sizeof tabStep / sizeof(int)) - 1;
int idxStep = 0;


uint8_t rssi = 0;
uint8_t snr = 0;
uint8_t stereo = 1;
uint8_t volume = DEFAULT_VOLUME;

// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
SI4735 rx;

void setup()
{
  // Encoder pins
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(BANDWIDTH_BUTTON, INPUT_PULLUP);
  pinMode(BAND_BUTTON, INPUT_PULLUP);
  pinMode(SEEK_BUTTON, INPUT_PULLUP);
  pinMode(BFO_SWITCH, INPUT_PULLUP);
  pinMode(AGC_SWITCH, INPUT_PULLUP);
  pinMode(STEP_SWITCH, INPUT_PULLUP);
  pinMode(MODE_SWITCH, INPUT_PULLUP);

  // Comment the line below if you do not have external audio mute circuit
  rx.setAudioMuteMcuPin(AUDIO_MUTE);

  // Use this initializer if using a 1.8" TFT screen:
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  // tft.setTextColor(ST77XX_BLUE);
  tft.setRotation(1);
  showTemplate();

  // Encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

  // rx.setup(RESET_PIN, 1); // Starts FM mode and ANALOG audio mode
  // rx.setup(RESET_PIN, -1, 1, SI473X_ANALOG_AUDIO); // Starts FM mode and ANALOG audio mode.
  rx.setup(RESET_PIN, -1, 1, SI473X_ANALOG_DIGITAL_AUDIO); // Starts FM mode and ANALOG and DIGITAL audio mode.

  // Set up the radio for the current band (see index table variable bandIdx )
  useBand();
  rx.setVolume(volume);
  showStatus();
}


/**
   Set all command flags to false
   When all flags are disabled (false), the encoder controls the frequency
*/
void disableCommands() {

  cmdBand = false;
  cmdBfo = false;
  bfoOn = false;
  cmdVolume = false;
  cmdAgc = false;
  cmdBandwidth = false;
  cmdStep = false;
  cmdMode = false;


}

/*
   Shows the static content on  display
*/
void showTemplate()
{

  int maxX1 = tft.width() - 2;
  int maxY1 = tft.height() - 5;

  tft.fillScreen(ST77XX_BLACK);

  tft.drawRect(2, 2, maxX1, maxY1, ST77XX_YELLOW);
  tft.drawLine(2, 40, maxX1, 40, ST77XX_YELLOW);
  tft.drawLine(2, 60, maxX1, 60, ST77XX_YELLOW);

}

/*
  Prevents blinking during the frequency display.
  Erases the old digits if it has changed and print the new digit values.
*/
void printValue(int col, int line, char *oldValue, char *newValue, uint8_t space, uint16_t color, uint8_t txtSize)
{
  int c = col;
  char *pOld;
  char *pNew;

  tft.setTextSize(txtSize);

  pOld = oldValue;
  pNew = newValue;

  // prints just changed digits
  while (*pOld && *pNew)
  {
    if (*pOld != *pNew)
    {
      // Erases olde value
      tft.setTextColor(ST77XX_BLACK);
      tft.setCursor(c, line);
      tft.print(*pOld);
      // Writes new value
      tft.setTextColor(color);
      tft.setCursor(c, line);
      tft.print(*pNew);
    }
    pOld++;
    pNew++;
    c += space;
  }

  // Is there anything else to erase?
  tft.setTextColor(ST77XX_BLACK);
  while (*pOld)
  {
    tft.setCursor(c, line);
    tft.print(*pOld);
    pOld++;
    c += space;
  }

  // Is there anything else to print?
  tft.setTextColor(color);
  while (*pNew)
  {
    tft.setCursor(c, line);
    tft.print(*pNew);
    pNew++;
    c += space;
  }

  // Save the current content to be tested next time
  strcpy(oldValue, newValue);
}


/*
    Reads encoder via interrupt
    Use Rotary.h and  Rotary.cpp implementation to process encoder via interrupt
*/
void rotaryEncoder()
{ // rotary encoder events
  uint8_t encoderStatus = encoder.process();

  if (encoderStatus)
    encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
}

/*
   Shows frequency information on Display
*/
void showFrequency()
{
  uint16_t color;
  char tmp[15];

  // It is better than use dtostrf or String to save space.

  sprintf(tmp, "%5.5u", currentFrequency);

  if (rx.isCurrentTuneFM())
  {
    bufferDisplay[0] = tmp[0];
    bufferDisplay[1] = tmp[1];
    bufferDisplay[2] = tmp[2];
    bufferDisplay[3] = '.';
    bufferDisplay[4] = tmp[3];
    bufferDisplay[5] = '\0';
    color = ST7735_CYAN;
  }
  else
  {
    if ( currentFrequency  < 1000 ) {
      bufferDisplay[0] = ' ';
      bufferDisplay[1] = ' ';
      bufferDisplay[2] = tmp[2] ;
      bufferDisplay[3] = tmp[3];
      bufferDisplay[4] = tmp[4];
      bufferDisplay[5] = '\0';
    } else {
      bufferDisplay[0] = (tmp[0] == '0') ? ' ' : tmp[0];
      bufferDisplay[1] = tmp[1];
      bufferDisplay[2] = tmp[2];
      bufferDisplay[3] = tmp[3];
      bufferDisplay[4] = tmp[4];
      bufferDisplay[5] = '\0';
    }
    color = (bfoOn) ? ST7735_CYAN: ST77XX_YELLOW;
  }

  printValue(30, 10, bufferFreq, bufferDisplay, 18, color, 2);
}


// Will be used by seekStationProgress function.
// This Si4735 library method calls the function below during seek process informing the current seek frequency.
void showFrequencySeek(uint16_t freq)
{
  currentFrequency = freq;
  showFrequency();
}

/*
    Show some basic information on display
*/
void showStatus()
{
  char *unt;

  int maxX1 = tft.width() - 5;

  tft.fillRect(3, 3, maxX1, 36, ST77XX_BLACK);
  tft.fillRect(3, 61, maxX1, 60, ST77XX_BLACK);

  CLEAR_BUFFER(bufferFreq);
  CLEAR_BUFFER(bufferUnt);
  CLEAR_BUFFER(bufferBand);
  CLEAR_BUFFER(bufferAGC);
  CLEAR_BUFFER(bufferBW);
  CLEAR_BUFFER(bufferStepVFO);
  CLEAR_BUFFER(bufferStereo);

  showFrequency();
  if (rx.isCurrentTuneFM()) {
    unt = (char *) "MHz";
  } else
  {
    unt = (char *) "KHz";
    showStep();
    showAgcAtt();
    if (loadSSB) showBFO();
  }
  printValue(140, 5, bufferUnt, unt, 6, ST77XX_GREEN,1);
  sprintf(bufferDisplay, "%s %s", band[bandIdx].bandName, bandModeDesc[currentMode]);
  printValue(5, 65, bufferBand, bufferDisplay, 6, ST77XX_CYAN, 1);

  showBandwitdth();
  /*
    rx.getStatus();
    rx.getCurrentReceivedSignalQuality();
    // SRN
    currentFrequency = rx.getFrequency();


    tft.setFont(Terminal6x8);
    printValue(155, 10, bufferStepVFO, bufferDisplay, ST77XX_BLACK, 7);

    if (rx.isCurrentTuneFM())
    {
      tft.drawText(155, 30, "MHz", ST77XX_RED);
      tft.drawText(124, 45, bufferBW, ST77XX_BLACK);
      CLEAR_BUFFER(bufferBW)
    }
    else
    {
      tft.fillRectangle(153, 3, 216, 20, ST77XX_BLACK);  // Clear Step field
      sprintf(bufferDisplay, "Stp: %3d", currentStep);
      printValue(153, 10, bufferStepVFO, bufferDisplay, ST77XX_YELLOW, 6);
      tft.drawText(153, 30, "KHz", ST77XX_RED);
    }

    if (band[bandIdx].bandType == SW_BAND_TYPE)
      sprintf(bufferDisplay, "%s %s", band[bandIdx].bandName, bandModeDesc[currentMode]);
    else
      sprintf(bufferDisplay, "%s", band[bandIdx].bandName);
    printValue(4, 60, bufferBand, bufferDisplay, ST77XX_CYAN, 6);

    // AGC
    rx.getAutomaticGainControl();
    sprintf(bufferDisplay, "%s %2d", (rx.isAgcEnabled()) ? "AGC" : "ATT", agcNdx);
    printValue(65, 60, bufferAGC, bufferDisplay, ST77XX_CYAN, 6);
    showFilter();
  */
}

void showBandwitdth() {
    // Bandwidth
    if (currentMode == LSB || currentMode == USB || currentMode == AM) {
      char * bw;

      if (currentMode == AM) 
        bw = (char *) bandwitdthAM[bwIdxAM];
      else 
        bw = (char *) bandwitdthSSB[bwIdxSSB];
      sprintf(bufferDisplay, "BW: %s KHz", bw);
    } 
    else {
      bufferDisplay[0] = '\0';
    }
    printValue(5, 110, bufferBW, bufferDisplay, 6, ST77XX_GREEN,1);
}

/* *******************************
   Shows RSSI status
*/
void showRSSI()
{
    int rssiLevel;
    int snrLevel;
    char sSt[15];
    int maxAux = tft.width() - 5;

    if (currentMode == FM)
    {
      sprintf(sSt, "%s", (rx.getCurrentPilot()) ? "ST" : "MO");
      printValue(4, 4, bufferStereo, sSt, 6, ST77XX_GREEN, 1);
    }

    // Check it
    // RSSI: 0 to 127 dBuV
    rssiLevel = map(rssi, 0, 127, 0, (maxAux - 48) );
    snrLevel = map(snr, 0, 127, 0, (maxAux - 48));

    // tft.fillRect(3, 3, maxX1, 36, ST77XX_BLACK);

    tft.fillRect(5, 42,  maxAux, 6, ST77XX_BLACK);
    tft.fillRect(5, 42, rssiLevel, 6, ST77XX_ORANGE);

    tft.fillRect(5, 51, maxAux, 6, ST77XX_BLACK);
    tft.fillRect(5, 51, snrLevel, 6, ST77XX_WHITE);
}

/**
   Shows the current AGC and Attenuation status
*/
void showAgcAtt() {
    char sAgc[15];
    rx.getAutomaticGainControl();
    if (agcNdx == 0 && agcIdx == 0)
      strcpy(sAgc, "AGC ON");
    else
    sprintf(sAgc, "ATT: %2d", agcNdx);
    tft.setFont(NULL);
    printValue(110, 110, bufferAGC, sAgc, 6, ST77XX_GREEN, 1);
}


/**
   Shows the current step
*/
void showStep() {
  char sStep[15];
  sprintf(sStep, "Stp:%3d", currentStep);
  printValue(110, 65, bufferStepVFO, sStep, 6, ST77XX_GREEN, 1);
}

void clearBFO() {
  // tft.fillRectangle(124, 52, 218, 79, ST77XX_BLACK); // Clear All BFO area
  CLEAR_BUFFER(bufferBFO);
}

void showBFO()
{
    sprintf(bufferDisplay, "%+4d", currentBFO);
    printValue(125, 30, bufferBFO, bufferDisplay, 7, ST77XX_CYAN,1);
}


/**
   Sets Band up (1) or down (!1)
*/
void setBand(int8_t up_down) {
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStep = currentStep;
  if ( up_down == 1)
    bandIdx = (bandIdx < lastBand) ? (bandIdx + 1) : 0;
  else
    bandIdx = (bandIdx > 0) ? (bandIdx - 1) : lastBand;
  useBand();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}


/*
   This function loads the contents of the ssb_patch_content array into the CI (Si4735) and starts the radio on
   SSB mode.
*/

void loadSSB()
{
  rx.reset();
  rx.queryLibraryId(); // Is it really necessary here? I will check it.
  rx.patchPowerUp();
  delay(50);
  rx.setI2CFastMode(); // Recommended
  // rx.setI2CFastModeCustom(500000); // It is a test and may crash.
  rx.downloadPatch(ssb_patch_content, size_content);
  rx.setI2CStandardMode(); // goes back to default (100KHz)

  // Parameters
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2KHz (default); 1=2.2KHz; 2=3KHz; 3=4KHz; 4=500Hz; 5=1KHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
  rx.setSSBConfig(bwIdxSSB, 1, 0, 0, 0, 1);
  delay(25);
  ssbLoaded = true;
}

/*
   Switch the radio to current band
*/
void useBand()
{
  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    currentMode = FM;
    rx.setTuneFrequencyAntennaCapacitor(0);
    rx.setFM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
    rx.setSeekFmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
    bfoOn = ssbLoaded = false;
    rx.setRdsConfig(1, 2, 2, 2, 2);
  }
  else
  {
    // set the tuning capacitor for SW or MW/LW
    rx.setTuneFrequencyAntennaCapacitor( (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) ? 0 : 1);

    if (ssbLoaded)
    {
      rx.setSSB(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep, currentMode);
      rx.setSSBAutomaticVolumeControl(1);
    }
    else
    {
      currentMode = AM;
      rx.setAM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
      bfoOn = false;
    }
    rx.setAmSoftMuteMaxAttenuation(0); // Disable Soft Mute for AM or SSB
    rx.setAutomaticGainControl(disableAgc, agcNdx);
    rx.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq); // Consider the range all defined current band
    rx.setSeekAmSpacing((band[bandIdx].currentStep > 10) ? 10 : band[bandIdx].currentStep); // Max 10KHz for spacing
  }
  delay(100);
  currentFrequency = band[bandIdx].currentFreq;
  currentStep = band[bandIdx].currentStep;
  rssi = 0;
  showStatus();
}


/**
   Switches the Bandwidth
*/
void doBandwidth(int8_t v) {
  if (currentMode == LSB || currentMode == USB)
  {
    bwIdxSSB = ( v == 1) ? bwIdxSSB + 1 : bwIdxSSB - 1;

    if (bwIdxSSB > 5)
      bwIdxSSB = 0;
    else if ( bwIdxSSB < 0 )
      bwIdxSSB = 5;

    rx.setSSBAudioBandwidth(bwIdxSSB);
    // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
    if (bwIdxSSB == 0 || bwIdxSSB == 4 || bwIdxSSB == 5)
      rx.setSBBSidebandCutoffFilter(0);
    else
      rx.setSBBSidebandCutoffFilter(1);
  }
  else if (currentMode == AM)
  {
    bwIdxAM = ( v == 1) ? bwIdxAM + 1 : bwIdxAM - 1;

    if (bwIdxAM > 6)
      bwIdxAM = 0;
    else if ( bwIdxAM < 0)
      bwIdxAM = 6;

    rx.setBandwidth(bwIdxAM, 1);
  }
  showBandwitdth();
  elapsedCommand = millis();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.

}



/**
   Deal with AGC and attenuattion
*/
void doAgc(int8_t v) {

  agcIdx = (v == 1) ? agcIdx + 1 : agcIdx - 1;
  if (agcIdx < 0 )
    agcIdx = 35;
  else if ( agcIdx > 35)
    agcIdx = 0;

  disableAgc = (agcIdx > 0);
  agcNdx = agcIdx;

  rx.setAutomaticGainControl(disableAgc, agcNdx);
  showAgcAtt();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();

}

/**
   Gets the current step index.
*/
int getStepIndex(int st) {
  for (int i = 0; i < lastStep; i++) {
    if ( st == tabStep[i] ) return i;
  }
  return 0;
}

/**
   Switches the current step
*/
void doStep(int8_t v) {
  idxStep = ( v == 1 ) ? idxStep + 1 : idxStep - 1;
  if ( idxStep > lastStep)
     idxStep = 0;
  else if ( idxStep < 0 )
     idxStep = lastStep;

  currentStep = tabStep[idxStep];

  rx.setFrequencyStep(currentStep);
  band[bandIdx].currentStep = currentStep;
  rx.setSeekAmSpacing((currentStep > 10) ? 10 : currentStep); // Max 10KHz for spacing
  showStep();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}

/**
   Switches to the AM, LSB or USB modes
*/
void doMode(int8_t v) {
  bufferBFO[0] =  bufferFreq[0] = '\0';
  bufferBFO[0];
  if (currentMode != FM)
  {
    if (currentMode == AM)
    {
      // If you were in AM mode, it is necessary to load SSB patch (avery time)
      loadSSB();
      currentMode = LSB;
    }
    else if (currentMode == LSB)
    {
      currentMode = USB;
    }
    else if (currentMode == USB)
    {
      currentMode = AM;
      bfoOn = ssbLoaded = false;
    }
    // Nothing to do if you are in FM mode
    band[bandIdx].currentFreq = currentFrequency;
    band[bandIdx].currentStep = currentStep;
    useBand();
  }

  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}

/**
   Find a station. The direction is based on the last encoder move clockwise or counterclockwise
*/
void doSeek() {
  rx.seekStationProgress(showFrequencySeek, seekDirection);
  currentFrequency = rx.getFrequency();
}


void loop()
{
  // Check if the encoder has moved.
  if (encoderCount != 0)
  {
    if (bfoOn)
    {
      currentBFO = (encoderCount == 1) ? (currentBFO + currentBFOStep) : (currentBFO - currentBFOStep);
      rx.setSSBBfo(currentBFO);
      showBFO();
      elapsedCommand = millis();
    }
    else if ( cmdMode ) 
      doMode(encoderCount);
    else if (cmdStep)
      doStep(encoderCount);
    else if (cmdAgc )
      doAgc(encoderCount);
    else if (cmdBandwidth)
      doBandwidth(encoderCount);
    else if (cmdBand)
      setBand(encoderCount);
    else
    {
      if (encoderCount == 1) {
        rx.frequencyUp();
        seekDirection = 1;
      }
      else {
        rx.frequencyDown();
        seekDirection = 0;
      }
      // Show the current frequency only if it has changed
      currentFrequency = rx.getFrequency();
    }
    showFrequency();
    encoderCount = 0;
  }
  else
  {
    if (digitalRead(BANDWIDTH_BUTTON) == LOW) {
      cmdBandwidth = true;
      elapsedCommand = millis();
    }
    else if (digitalRead(BAND_BUTTON) == LOW) {
      cmdBand = true;
      elapsedCommand = millis();
    }
    else if (digitalRead(SEEK_BUTTON) == LOW) {
      doSeek();
    }
    else if (digitalRead(BFO_SWITCH) == LOW) {
      bfoOn = !bfoOn;
      cmdBfo = false;
      elapsedCommand = millis();
      delay(MIN_ELAPSED_TIME);
    }
    else if (digitalRead(AGC_SWITCH) == LOW) {
        cmdAgc = true;
        elapsedCommand = millis();
    }
    else if (digitalRead(STEP_SWITCH) == LOW) {
        cmdStep = true;
        elapsedCommand = millis();
    }
    else if (digitalRead(MODE_SWITCH) == LOW) {
        cmdMode = true;
        elapsedCommand = millis();
    }
  }


  // Show RSSI status only if this condition has changed
  if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 6)
  {
    rx.getCurrentReceivedSignalQuality();
    int aux = rx.getCurrentRSSI();
    if (rssi != aux)
    {
      rssi = aux;
      snr = rx.getCurrentSNR();
      showRSSI();
    }
    elapsedRSSI = millis();
  }

  // Disable commands control
  if ( (millis() - elapsedCommand) > ELAPSED_COMMAND ) {
    if ( cmdBfo ) {
      bufferFreq[0] = '\0';
      bfoOn = cmdBfo = false;
      showFrequency();
    }
    disableCommands();
    elapsedCommand = millis();
  }

  delay(5);
}