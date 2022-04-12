/*

This is an Arduino project.

ABOUT:
I'm an IR lap timer, you can use me to count and record lap times... eg: of your RC car model!

You can use 4 hardware normally open push buttons or an old infrared remote control with at least 4 buttons.
If your remote control have an extra button, you can use it as "fake-a-lap" feature.
If you attach a passive buzzer you can enjoy the "pressure" feature.

HARDWARE:
(mandatory)
microcontroller (atmega328)
IR receiver     (tsop31236)
display 128x64  (SSD1306 or others supported by U8X8 library)
(optional)
buzzer          (any passive buzzer)
4 buttons       (any N.O. button)
or an old infrared remote control with at least 4 buttons

USER INTERFACE:
OK = enter a submenu / change an option / start a session
CANCEL = exit a submenu / stop a session
UP = go to the next element
DOWN = go to the previous element

MENU:
main (overall summary)
-> CANCEL - memory reset -> OK/CANCEL
-> OK - start new session -> OK -> recording... -> CANCEL/FakeLAP/RealLAP
      - UP - audio -> OK to toggle
      - UP - pressure -> OK to toggle
-> UP - session summary -> OK -> lap details -> UP -> next lap of the recorded session

you have to customize getKeys() for:
- FakeLAP
you can use a fifth button on your infrared remote control or an extra hardware push button.
- RealLAP
use a different device programmed with the "simple_ir_sender" sketch or something similar.

TODO:
- refactoring of the whole stuff removing unused or duplicated things
- option to disable eeprom writes
- option to dinamically change the current active transponder (RealLAP)

*/

#include <EEPROM.h>
//#include <LiquidCrystal.h>
#include "laptimes.h"
#include "pitches.h"

//*******************************************************************************************
// USER INTERFACE DEFINITIONS 
//*******************************************************************************************
// initialise the liquidCrystal Library 
// I am using the analog pins to drive the LCD, they work like digital pins if your not using
// analogRead, the names A0 to A5 provide and easy way to use the analog pins in digital 
// functions - i.e. digitalWrite(A0);digitalRead(A1); etc
//LiquidCrystal lcd(A5,A4,A3,A2,A1,A0);
#include "src/U8g2lib.h"
#define MISOpin 6
#define SCLKpin 8
#define SSpin   7
//U8G2_ST7920_128X64_2_SW_SPI lcd(U8G2_R2, SCLKpin, MISOpin, SSpin, /* reset=*/ U8X8_PIN_NONE);
//U8G2_SH1106_128X64_NONAME_1_HW_I2C lcd(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display
U8X8_SSD1306_128X64_NONAME_HW_I2C lcd(/* reset=*/ U8X8_PIN_NONE); 	      

#include "PinDefinitionsAndMore.h"
#include <IRremote.h>

// PINs for user interface buttons - use any
#define KEY_OK_PIN 	14
#define KEY_CANCEL_PIN 	15
#define KEY_UP_PIN 	16
#define KEY_DOWN_PIN 	17

// bit flags used in key functions getKeys, waitForKeyPress, waitForKeyRelease
#define KEY_NONE 	0
#define KEY_OK 		1
#define KEY_CANCEL 	2
#define KEY_UP 		4
#define KEY_DOWN 	8

#define KEYPRESS_ANY B11111111

// display width + 1, used by getRamString to copy a PROG_MEM string into ram
#define DISPLAY_ROW_BUFFER_LENGTH 17

//*******************************************************************************************
// Lap Capture definitions
//*******************************************************************************************
#define BUZZER_PIN 13 //a led or active buzzer
#define SPEAKER_PIN 9 //a passive buzzer used with the tone() function
#define NOTE_SUSTAIN 50


// minimum and maximum duration of qualifying IR Pulse
//#define MIN_PULSE_DURATION 6000L
//#define MAX_PULSE_DURATION 10000L
#define MIN_PULSE_DURATION 20L
#define MAX_PULSE_DURATION 2000L

#define SETTING_START_RECORDING 0
#define SETTING_EXTERNAL_AUDIO 1
#define SETTING_PRESSURE_MODE 2
#define SETTING_LAST 2
 
bool bPressureOn = true;
bool bExternalAudio = true;

// start and end of pulse
uint32_t ulStartPulse;
uint32_t ulEndPulse;
volatile uint32_t ulPulseDuration;

// flags to manage access and pulse edges
volatile uint8_t bIRPulseFlags;
//
volatile uint32_t ulNewLapStartTime;

#define IR_PULSE_START_SET 1
#define IR_PULSE_END_SET 2

//*****************************************************************
// Global Instance of CLapTimes class
//*****************************************************************
CLapTimes gLapTimes(new CEEPROMLapStore());


char * getRamString(PGM_P pString)
{
  // NEED TO ADD A CHECK HERE TO ENSURE pString < DISPLAY_ROW_LENGTH
  
  static char pBuffer[DISPLAY_ROW_BUFFER_LENGTH];
  
  return strcpy_P(pBuffer,pString);
}

/*
void lcd_prepare(void) {
  lcd.setFont(u8g2_font_6x10_tf);
//  lcd.setFont(u8g2_font_5x8_mr);
  //lcd.setFont(u8g2_font_pxplustandynewtv_8r);
  //lcd.setFont(u8g2_font_courR08_tr);
//  lcd.setFont(u8g2_font_amstrad_cpc_extended_8r);
  //lcd.setFont(u8g2_font_p01type_tr);
  lcd.setFontRefHeightExtendedText();
  lcd.setDrawColor(1);
  lcd.setFontPosTop();
  //lcd.setFontPosBaseline();
  lcd.setFontDirection(0);
}
*/

//////////////////////////////////////////////////////////////////////////////////
//
// doShowSessionSummaries
//
// implements the show session summary menu
// allows the user to scroll up and down through summaries of the recorded sessions
//
//////////////////////////////////////////////////////////////////////////////////
void setup()
{
 Serial.begin(9600);
 Serial.println(getRamString(PSTR("In Setup")));

  pinMode(LED_BUILTIN, OUTPUT);

#if defined(__AVR_ATmega32U4__) || defined(SERIAL_USB) || defined(SERIAL_PORT_USBVIRTUAL)  || defined(ARDUINO_attiny3217)
  delay(4000); // To be able to connect Serial monitor after reset or power up and before first print out. Do not wait for an attached Serial Monitor!
#endif
  // Just to know which program is running on my Arduino
  Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver, enable feedback LED, take LED feedback pin from the internal boards definition
//  IR::initialise(0);
  
  Serial.print(F("Ready to receive IR signals at pin "));
  Serial.println(IR_RECEIVE_PIN);

/*   
 lcd.begin(16, 2);
 lcd.print("RCArduino");
 lcd.setCursor(0,31);
 lcd.print("LapTimer 1.0");
*/

  lcd.begin();
//  lcd.setFont(u8x8_font_amstrad_cpc_extended_f);
//  lcd.setFont(u8x8_font_5x8_f);
  lcd.setFont(u8x8_font_5x8_r);
    lcd.setCursor(0,0);
    lcd.print(F(" LapTimer 1.0 "));

/*
  lcd.begin();
  // picture loop
  lcd.firstPage();
  do {
    lcd_prepare();
    lcd.setCursor(0,31);
    lcd.print(F(" LapTimer 1.0 "));
  } while( lcd.nextPage() );
*/

 delay(3000);
 
 pinMode(KEY_OK_PIN,INPUT_PULLUP);
 pinMode(KEY_CANCEL_PIN,INPUT_PULLUP);
 pinMode(KEY_UP_PIN,INPUT_PULLUP);
 pinMode(KEY_DOWN_PIN,INPUT_PULLUP);

 pinMode(SPEAKER_PIN,OUTPUT);
 pinMode(BUZZER_PIN,OUTPUT);

 digitalWrite(SPEAKER_PIN,LOW);
 digitalWrite(BUZZER_PIN,LOW);
 noTone(BUZZER_PIN);
 noTone(SPEAKER_PIN);
 
 showTotals();  
  
 Serial.println(getRamString(PSTR("Out Setup")));
}

//////////////////////////////////////////////////////////////////////////////////
//
// base loop, implements root of menu system
//
// allows the user to scroll up and down through summaries of the recorded sessions
//
//////////////////////////////////////////////////////////////////////////////////
void loop()
{  
  // lets keep control of the loop
//  while(true)
//  {
   // wait for a key command to tell us what to do
   Serial.println(getRamString(PSTR("Beginning Loop")));
   switch(waitForKeyPress(KEYPRESS_ANY))
   {
    // start recording
    case KEY_OK:
      doSettings();
     // doRecord();
     break;
    // delete all sessions 
    case KEY_CANCEL:
     doConfirmDeleteSessions();
     break;
    // scroll through recorded session summaries
    case KEY_UP:
    case KEY_DOWN:
     doShowSessionSummaries();
     break;
   }

   showTotals();
    
   waitForKeyRelease();
//  }
}

void doSettings()
{
  uint8_t sKeys = KEY_NONE;
  uint8_t sSetting = SETTING_START_RECORDING;
  uint8_t bExit = false;
  
  lcd.clear();
  lcd.setCursor(0,0);
  
  do
  {
    switch(sKeys)
    {
      case KEY_UP:
        if(sSetting < SETTING_LAST)
        {
          sSetting++;
        }
        break;
      case KEY_DOWN:
        if(sSetting > 0)
        { 
          sSetting--;
        }
        break;
      case KEY_OK:
        if(sSetting == SETTING_START_RECORDING)
        {
          doRecord();
          bExit = true;
        }
        toggleSetting(sSetting);
        break;
    }
    waitForKeyRelease();
    if(!bExit)
    {
      showSetting(sSetting);
      sKeys = waitForKeyPress(KEYPRESS_ANY);
    }
  }
  while((!bExit) && (sKeys != KEY_CANCEL));
}

void toggleSetting(uint8_t sSetting)
{
  switch(sSetting)
  {
    case SETTING_EXTERNAL_AUDIO:
      bExternalAudio = (!bExternalAudio);
      break;
    case SETTING_PRESSURE_MODE:
      bPressureOn = (!bPressureOn);
      break;
  }
}

void showSetting(uint8_t sSetting)
{
  static const char pstrOn[] = "ON"; 
  static const char pstrOff[] = "OFF";
    
  lcd.clear();
  lcd.setCursor(0,0);
      
  switch(sSetting)
  {
    case SETTING_START_RECORDING:
      lcd.print(getRamString(PSTR("OK TO START")));
      Serial.println(getRamString(PSTR("OK TO START")));
      break;
    case SETTING_EXTERNAL_AUDIO:
      lcd.print(getRamString(PSTR("EXTERNAL AUDIO")));
      Serial.println(getRamString(PSTR("EXTERNAL AUDIO")));
      lcd.setCursor(0,2);

      if(bExternalAudio)
      {
        lcd.print(pstrOn);
        Serial.println(pstrOn);
      }
      else
      {
        lcd.print(pstrOff);
        Serial.println(pstrOff);
      }
      break;
    case SETTING_PRESSURE_MODE:
      lcd.print(getRamString(PSTR("PRESSURE MODE")));
      Serial.println(getRamString(PSTR("PRESSURE MODE")));
      lcd.setCursor(0,2);

      if(bPressureOn)
      {
        lcd.print(pstrOn);
        Serial.println(pstrOn);
      }
      else
      {
        lcd.print(pstrOff);
        Serial.println(pstrOff);
      }
  }
  
//  lcd.sendBuffer();
}

//////////////////////////////////////////////////////////////////////////////////
//
// doRecord
//
// start recording new sessions, update screen every second
//
//////////////////////////////////////////////////////////////////////////////////
void doRecord()
{
  lap_handle_t currentLapHandle = gLapTimes.createNewSession();
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(getRamString(PSTR("Countdown")));
  Serial.println(getRamString(PSTR("Countdown")));
  
  uint8_t sAudioPin = BUZZER_PIN;
  if(bExternalAudio)
  {
    sAudioPin = SPEAKER_PIN;
  }

  delay(1000);
  lcd.clear();  

  tone(sAudioPin,NOTE_C4,200);
  lcd.setCursor(0,0);
  lcd.print(getRamString(PSTR("4")));
  delay(1000);
  tone(sAudioPin,NOTE_C4,200);
  lcd.setCursor(0,1);
  lcd.print(getRamString(PSTR("3")));
  delay(1000);
  tone(sAudioPin,NOTE_C4,200);
  lcd.setCursor(0,2);
  lcd.print(getRamString(PSTR("2")));
  delay(1000);
  tone(sAudioPin,NOTE_C4,200);
  lcd.setCursor(0,3);
  lcd.print(getRamString(PSTR("1")));
  delay(1000);
  tone(sAudioPin,NOTE_E4,400);
  lcd.clear();
  lcd.setCursor(5,2);
  lcd.print(getRamString(PSTR("GO!!!")));
  delay(1000);
  noTone(sAudioPin);

//lcd.sendBuffer();

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(getRamString(PSTR("Recording")));
  Serial.println(getRamString(PSTR("Recording")));
//lcd.sendBuffer();
  
  uint32_t ulOldLapStartTime = millis();  
  lap_time_t bestLapTime = 0XFFFF;
  lap_time_t countdownTime = bestLapTime - 400;
  ulNewLapStartTime = ulOldLapStartTime;
  ulStartPulse = 0;
  ulEndPulse = 0;
  ulPulseDuration = ulEndPulse - ulStartPulse;
  bIRPulseFlags = 0;

 uint16_t nSessions = 0;
 uint16_t nLapsRecorded = 0;
 uint16_t nLapsRemaining = 0;
  uint16_t nSessionAverage = 0;
  uint16_t nSessionBest = 0;
  uint16_t nSessionLapCount = 0;
  uint16_t nSessionTotal = 0;

  uint32_t ulLastTimeRefresh = millis();
  char *pStringTimeBuffer = NULL;

//  attachInterrupt(digitalPinToInterrupt(IR_RECEIVE_PIN),captureLap,CHANGE);
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver, enable feedback LED, take LED feedback pin from the internal boards definition
  
  while((getKeys() != KEY_CANCEL) && (currentLapHandle != INVALID_LAP_HANDLE))
  {
//    Serial.print(getRamString(PSTR("ulPulseDuration ")));
//    Serial.println(ulPulseDuration);    
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    // Check for new laps captured
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    if((IR_PULSE_END_SET|IR_PULSE_START_SET) == bIRPulseFlags)
    {
      uint32_t ulLastLapDuration = ulNewLapStartTime - ulOldLapStartTime;
      lap_time_t lapTime = CLapTimes::convertMillisToLapTime(ulLastLapDuration); 
    Serial.print(getRamString(PSTR("lapTime ")));
    Serial.println(lapTime);    
        
      // ignore laps less than 10 seconds long
      if(lapTime > 1000)
      {
        ulOldLapStartTime = ulNewLapStartTime;
    
        gLapTimes.addLapTime(currentLapHandle,lapTime);        
        currentLapHandle = gLapTimes.moveNext(currentLapHandle);        

  gLapTimes.getTotals(nSessions,nLapsRecorded,nLapsRemaining);
        Serial.print(getRamString(PSTR(" nSessions: ")));
        Serial.println(nSessions);
  lap_handle_t currentSessionHandle = 0;
  currentSessionHandle = gLapTimes.getSessionHandle(nSessions-1);  

    gLapTimes.getSessionSummary(currentSessionHandle,nSessionAverage,nSessionBest,nSessionLapCount,nSessionTotal);
        
        countdownTime = bestLapTime - 200;
        
        lcd.clear();
        
/*
        lcd.setCursor(0,1);
        lcd.print(getRamString(PSTR("Best")));
        Serial.print(getRamString(PSTR("Best Lap ")));
        lcd.print(CLapTimes::formatTime(bestLapTime,true));
        Serial.println(CLapTimes::formatTime(bestLapTime,true));
*/
       lcd.setCursor(0,1);
       lcd.print(CLapTimes::formatTime(bestLapTime,true));
       Serial.print(CLapTimes::formatTime(bestLapTime,true));
       lcd.print(getRamString(PSTR(" Best")));
       Serial.println(getRamString(PSTR(" Best")));
  
        lcd.setCursor(0,0);
        // use this to show lap time
        lcd.print(CLapTimes::formatTime(lapTime,true));
        Serial.println(CLapTimes::formatTime(lapTime,true));
//        lcd.print(getRamString(PSTR(" Last")));
        Serial.println(getRamString(PSTR(" Last")));

        // new best lap      
        if(lapTime < bestLapTime)
        {
          // and this to show delta time
//          lcd.setCursor(0,3);
          lcd.print(getRamString(PSTR(" -")));
          lcd.print(CLapTimes::formatTime(bestLapTime-lapTime,true));
          Serial.println(CLapTimes::formatTime(bestLapTime-lapTime,true));
          bestLapTime = lapTime;
        }else
        {
          // and this to show delta time
//          lcd.setCursor(0,3);
          lcd.print(getRamString(PSTR(" +")));
          lcd.print(CLapTimes::formatTime(lapTime-bestLapTime,true));
          Serial.println(CLapTimes::formatTime(lapTime-bestLapTime,true));
        }

       lcd.setCursor(0,2);
       lcd.print(CLapTimes::formatTime(nSessionAverage,true));
       Serial.print(CLapTimes::formatTime(nSessionAverage,true));
       lcd.print(getRamString(PSTR(" AVG")));
       Serial.println(getRamString(PSTR(" AVG")));
      
       lcd.setCursor(0,3);
       lcd.print(CLapTimes::formatTime(nSessionTotal,true));
       Serial.print(CLapTimes::formatTime(nSessionTotal,true));
       lcd.print(getRamString(PSTR(" TOT")));
       Serial.println(getRamString(PSTR(" TOT")));
  
        lcd.setCursor(0,4);
        lcd.print(getRamString(PSTR(" Laps: ")));
        Serial.println(getRamString(PSTR(" Laps: ")));
        lcd.print(nSessionLapCount);
        Serial.println(nSessionLapCount);

  // Dont do this, play a tune !
       if(lapTime == bestLapTime)
       {
         for(uint8_t nLoop = 0;nLoop < 2;nLoop ++)
         {
           tone(sAudioPin,NOTE_A5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_B5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_C5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_B5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_C5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_D5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_C5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_D5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_E5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_D5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_E5);
           delay(NOTE_SUSTAIN);
           tone(sAudioPin,NOTE_E5);
           delay(NOTE_SUSTAIN);
         }
         noTone(sAudioPin);
       }
       else
       {
           tone(sAudioPin,NOTE_G4);
           delay(250);
           tone(sAudioPin,NOTE_C4);
           delay(500);
           noTone(sAudioPin);
       }

    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver, enable feedback LED, take LED feedback pin from the internal boards definition
       delay(5000);       
  /*      digitalWrite(LAP_CAPTURE_LED,HIGH);
        digitalWrite(BUZZER_PIN,HIGH);
        delay(400);
        
        if(lapTime == bestLapTime)
        {
          digitalWrite(LAP_CAPTURE_LED,LOW);
          digitalWrite(BUZZER_PIN,LOW);
          delay(200);
          digitalWrite(LAP_CAPTURE_LED,HIGH);
          digitalWrite(BUZZER_PIN,HIGH);
          delay(400);
        }
        
        digitalWrite(LAP_CAPTURE_LED,LOW);
        digitalWrite(BUZZER_PIN,LOW);
                        
        // dont look for another lap for 2 seconds
        delay(2000);  
  */
      }
      
      // give ownership of the shared variables back to the ISR
      bIRPulseFlags = 0;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    // The countdown buzzer
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    // everytime a new best lap gets set, set countdownTime to bestLapTime - 400 
    if(bPressureOn)
    {
      lap_time_t currentTime = CLapTimes::convertMillisToLapTime(millis() - ulOldLapStartTime);
      if(currentTime >= countdownTime)
      {
        if(currentTime <= bestLapTime)
        {
          // count down 4,3,2,1 to best lap target
          tone(sAudioPin,NOTE_C5);
          delay(50);
          noTone(sAudioPin);
        }
        else
        {
          if(currentTime <= (bestLapTime + 300))
          {
             // count down 4,3,2,1 to best lap target
            tone(sAudioPin,NOTE_C2);
            delay(50);
            noTone(sAudioPin);
          }
        }
        countdownTime += 100; // add one second
        IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver, enable feedback LED, take LED feedback pin from the internal boards definition
      }
    }
            
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    // Update screen with current lap time
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    uint32_t ulCurrentLapTime = millis();
    if((ulCurrentLapTime - ulLastTimeRefresh) > 1000)
    {
      ulLastTimeRefresh = ulCurrentLapTime;
      
      lcd.clear();
      
      if(bestLapTime != 0XFFFF)
      {
       lcd.setCursor(0,1);
       lcd.print(CLapTimes::formatTime(nSessionBest,true));
       Serial.print(CLapTimes::formatTime(nSessionBest,true));
       lcd.print(getRamString(PSTR(" Best")));
       Serial.println(getRamString(PSTR(" Best")));

       lcd.setCursor(0,2);
       lcd.print(CLapTimes::formatTime(nSessionAverage,true));
       Serial.print(CLapTimes::formatTime(nSessionAverage,true));
       lcd.print(getRamString(PSTR(" AVG")));
       Serial.println(getRamString(PSTR(" AVG")));

       lcd.setCursor(0,3);
       lcd.print(CLapTimes::formatTime(nSessionTotal,true));
       Serial.print(CLapTimes::formatTime(nSessionTotal,true));
       lcd.print(getRamString(PSTR(" TOT")));
       Serial.println(getRamString(PSTR(" TOT")));
        }
      else
      {
       lcd.setCursor(0,2);
       lcd.print(getRamString(PSTR("Recording"))); 
       Serial.println(getRamString(PSTR("Recording"))); 
      }
      
      lap_time_t currentTime = CLapTimes::convertMillisToLapTime(ulCurrentLapTime - ulOldLapStartTime);
      pStringTimeBuffer = CLapTimes::formatTime(currentTime,false);
      if(pStringTimeBuffer != NULL)
      {
        lcd.setCursor(0,0);
        lcd.print(pStringTimeBuffer);
        Serial.println(pStringTimeBuffer);

        lcd.setCursor(0,4);
        lcd.print(getRamString(PSTR(" Laps: ")));
        Serial.print(getRamString(PSTR(" Laps: ")));
        lcd.print(nSessionLapCount);
        Serial.println(nSessionLapCount);
      }
      else
      {
        // If we do not complete a lap for 9m59s display an idle message until a key is pressed
        lcd.setCursor(0,0);
        lcd.print(getRamString(PSTR("Idle")));
        Serial.println(getRamString(PSTR("Idle")));
        waitForKeyPress(KEYPRESS_ANY);
        ulOldLapStartTime = millis();
      }
    }
  }
  detachInterrupt(digitalPinToInterrupt(IR_RECEIVE_PIN));
  
  if(currentLapHandle == INVALID_LAP_HANDLE)
  {
    lcd.setCursor(0,2);
    lcd.print(getRamString(PSTR("Memory Full!")));
    Serial.println(getRamString(PSTR("Memory Full!")));
  }
  
//  lcd.sendBuffer();
}

//////////////////////////////////////////////////////////////////////////////////
//
// doConfirmDeleteSessions
//
// Delete all sessions
//
//////////////////////////////////////////////////////////////////////////////////
void doConfirmDeleteSessions()
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(getRamString(PSTR("OK to Reset")));
  Serial.println(getRamString(PSTR("OK to Reset")));
  lcd.setCursor(0,2);
  lcd.print(getRamString(PSTR("CANCEL to keep")));
  Serial.println(getRamString(PSTR("CANCEL to keep")));
//lcd.sendBuffer();

  // we pressed cancel to get here - so lets wait for cancel to be released before we look for more input
  waitForKeyRelease();
  
  if(KEY_OK == waitForKeyPress(KEY_OK|KEY_CANCEL))
  {
   gLapTimes.clearAll();
  }
}

//////////////////////////////////////////////////////////////////////////////////
//
// doShowSessionSummaries
//
// implements the show session summary menu
// allows the user to scroll up and down through summaries of the recorded sessions
//
//////////////////////////////////////////////////////////////////////////////////
void doShowSessionSummaries()
{
 boolean bFinished = false;
 uint8_t nSession = 0;
 
 do
 {
  lap_handle_t lapHandle = 0;
  uint16_t nSessionAverage = 0;
  uint16_t nSessionBest = 0;
  uint16_t nSessionLapCount = 0;
  uint16_t nSessionTotal = 0;
 
  Serial.println(nSession);
   
  lapHandle = gLapTimes.getSessionHandle(nSession);  
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(getRamString(PSTR("Session: ")));
  Serial.print(getRamString(PSTR("Session: ")));
  lcd.print(nSession+1);
  Serial.println(nSession+1);
  
  // if theres no laps for this session or its the first session but it doesnt contain any laps
  if(lapHandle == INVALID_LAP_HANDLE || (lapHandle == 0 && gLapTimes.getLapTime(lapHandle)==0))
  {
    lcd.setCursor(0,2);
    lcd.print(getRamString(PSTR("Empty Session")));
    Serial.println(getRamString(PSTR("Empty Session")));
  }
  else
  {
    Serial.println(lapHandle);
    
    gLapTimes.getSessionSummary(lapHandle,nSessionAverage,nSessionBest,nSessionLapCount,nSessionTotal);

       lcd.setCursor(0,1);
       lcd.print(CLapTimes::formatTime(nSessionBest,true));
       Serial.print(CLapTimes::formatTime(nSessionBest,true));
       lcd.print(getRamString(PSTR(" Best")));
       Serial.println(getRamString(PSTR(" Best")));

       lcd.setCursor(0,2);
       lcd.print(CLapTimes::formatTime(nSessionAverage,true));
       Serial.print(CLapTimes::formatTime(nSessionAverage,true));
       lcd.print(getRamString(PSTR(" AVG")));
       Serial.println(getRamString(PSTR(" AVG")));
      
       lcd.setCursor(0,3);
       lcd.print(CLapTimes::formatTime(nSessionTotal,true));
       Serial.print(CLapTimes::formatTime(nSessionTotal,true));
       lcd.print(getRamString(PSTR(" TOT")));
       Serial.println(getRamString(PSTR(" TOT")));
      
        lcd.setCursor(0,4);
        lcd.print(getRamString(PSTR(" Laps: ")));
        Serial.print(getRamString(PSTR(" Laps: ")));
        lcd.print(nSessionLapCount);
        Serial.println(nSessionLapCount);

/*    
    lcd.print(getRamString(PSTR(" Laps: ")));
    Serial.print(getRamString(PSTR(" Laps: ")));
    lcd.print(nSessionLapCount);
    Serial.println(nSessionLapCount);
    lcd.setCursor(0,2);
// Best Lap Time
    lcd.print(CLapTimes::formatTime(nSessionBest,true));
    Serial.println(CLapTimes::formatTime(nSessionBest,true));
// Average Lap Time
    lcd.print(" ");
    Serial.println(" ");
    lcd.print(CLapTimes::formatTime(nSessionAverage,true));
    Serial.println(CLapTimes::formatTime(nSessionAverage,true));
*/
  }
//lcd.sendBuffer();
  waitForKeyRelease();

  switch(waitForKeyPress(KEYPRESS_ANY))
  {
   case KEY_UP:
    nSession++;
    break;
   case KEY_DOWN:
    nSession--;
    break;
   case KEY_CANCEL:
    bFinished = true;
    break;
   case KEY_OK:
    if(nSessionLapCount != 0)
    {  
      doLapScroll(gLapTimes.getSessionHandle(nSession));
    }
    break;
  }
 }while(!bFinished);
}

//////////////////////////////////////////////////////////////////////////////////
//
// showTotals shows the number of sessions, laps and laps left
// as the root of the menu
//
//////////////////////////////////////////////////////////////////////////////////
void showTotals()
{
 Serial.println(getRamString(PSTR("Entering showTotals")));
 
 uint16_t nSessions = 0;
 uint16_t nLapsRecorded = 0;
 uint16_t nLapsRemaining = 0;
 
 gLapTimes.getTotals(nSessions,nLapsRecorded,nLapsRemaining);
 
 lcd.clear();
 lcd.print(getRamString(PSTR("Sessions=")));lcd.print(nSessions);
 Serial.print(getRamString(PSTR("Sessions=")));Serial.println(nSessions);
 lcd.setCursor(0, 1);
 lcd.print(getRamString(PSTR("Laps=")));lcd.print(nLapsRecorded);
 Serial.print(getRamString(PSTR("Laps=")));Serial.println(nLapsRecorded);
 lcd.setCursor(0, 2);
 lcd.print(getRamString(PSTR("Left=")));lcd.print(nLapsRemaining);
 Serial.print(getRamString(PSTR("Left=")));Serial.println(nLapsRemaining);
//lcd.sendBuffer();
  
 Serial.println(getRamString(PSTR("Leaving showSummaryData")));
}

//////////////////////////////////////////////////////////////////////////////////
//
// doLapScroll
//
//////////////////////////////////////////////////////////////////////////////////
void doLapScroll(lap_handle_t startLapHandle)
{
 boolean bFinished = false;
 lap_handle_t currentLapHandle = startLapHandle;
 lap_handle_t tmpLap = currentLapHandle;
 uint8_t nLapNumber = 0;
 
 do
 {
   lcd.clear();
   lcd.setCursor(0,0);
   
   if(tmpLap == INVALID_LAP_HANDLE)
   {
     lcd.print(getRamString(PSTR("No More Laps")));
     Serial.println(getRamString(PSTR("No More Laps")));
//lcd.sendBuffer();
     delay(2000);
     lcd.clear();
   }

   lcd.print(getRamString(PSTR("Lap ")));
   Serial.print(getRamString(PSTR("Lap ")));
   lcd.print(nLapNumber+1);
   Serial.println(nLapNumber+1);
   lcd.setCursor(0,2);
   
   if(currentLapHandle != INVALID_LAP_HANDLE)
   {
     char *pTime = CLapTimes::formatTime(gLapTimes.getLapTime(currentLapHandle),true);
     lcd.setCursor(0,2);
     lcd.print(pTime);  
     Serial.println(pTime);  
   }
//lcd.sendBuffer();

   waitForKeyRelease();
   
   uint8_t sKey = waitForKeyPress(KEYPRESS_ANY);
   switch(sKey)
   {
     case KEY_DOWN:
     case KEY_UP:
      (sKey == KEY_UP) ? tmpLap = gLapTimes.moveNext(currentLapHandle) : tmpLap = gLapTimes.movePrevious(currentLapHandle);
      if(tmpLap != INVALID_LAP_HANDLE)
      {
        if(gLapTimes.getLapTime(tmpLap) != EMPTY_LAP_TIME)
        {
          currentLapHandle = tmpLap;
          (sKey == KEY_UP) ? nLapNumber++ : nLapNumber--;
        }
        else
        {
          tmpLap = INVALID_LAP_HANDLE;
        }
      }
      break;
     case KEY_OK:
      tmpLap = currentLapHandle;
      break;
     case KEY_CANCEL:
      bFinished = true;
      break;
   }
 }
 while(!bFinished);
}

//////////////////////////////////////////////////////////////////////////////////
//
// Key related helpers
//
// getKeys - pole keys
// waitForKeyPress - block waiting for keys based on a mask
// waitForKeyRelease - block waiting until no kets are pressed
//
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//
// getKeys
//
// read the inputs and create a bit mask based on the buttons pressed
// this does not block, need to review whether we should make this block, in most
// cases we loop waiting for a key, sometimes we also loop waiting for no key
// could put both options here with an input parameter.
//
//////////////////////////////////////////////////////////////////////////////////
short getKeys()
{
 // Use bit flags for keys, we may have a future use for
 // combined key presses

 short sKeys = KEY_NONE;
 if(digitalRead(KEY_UP_PIN) == LOW)
 {
  sKeys |= KEY_UP; 
 }
 if(digitalRead(KEY_DOWN_PIN) == LOW)
 {
  sKeys |= KEY_DOWN;
 }
 if(digitalRead(KEY_OK_PIN) == LOW)
 {
  sKeys |= KEY_OK;
 }
 if(digitalRead(KEY_CANCEL_PIN) == LOW)
 {
  sKeys |= KEY_CANCEL;
 }


//Address=0x31 Command=0x14 play
//Address=0x31 Command=0x10 stop
//Address=0x31 Command=0x1 up
//Address=0x31 Command=0x2 down


//    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver, enable feedback LED, take LED feedback pin from the internal boards definition
    if (IrReceiver.decode()) {  // Grab an IR code
        // Check if the buffer overflowed
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_WAS_OVERFLOW) {
            Serial.println("IR code too long. Edit IRremoteInt.h and increase RAW_BUFFER_LENGTH");
        }
        IrReceiver.resume();                            // Prepare for the next value
        
        // Finally, check the received data and perform actions according to the received command
        if (IrReceiver.decodedIRData.command == 0x1) {
            sKeys |= KEY_UP;
        } else if (IrReceiver.decodedIRData.command == 0x2) {
            sKeys |= KEY_DOWN;
        } else if (IrReceiver.decodedIRData.command == 0x14) {
            sKeys |= KEY_OK;
        } else if (IrReceiver.decodedIRData.command == 0x10) {
            sKeys |= KEY_CANCEL;
        } else if (IrReceiver.decodedIRData.command == 0x15) {
            Serial.println("Fake-a-Lap");
            captureLap_new();
            //captureLap();
            //ulNewLapStartTime = millis();
        } else if (IrReceiver.decodedIRData.command == 0x34) {
            Serial.println("Real-Lap A");
            captureLap_new();
            //captureLap();
            //ulNewLapStartTime = millis();
        } else {
            // Print a short summary of received data
            IrReceiver.printIRResultShort(&Serial);
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver, enable feedback LED, take LED feedback pin from the internal boards definition
        }
    } 

 return sKeys;
}

//////////////////////////////////////////////////////////////////////////////////
//
// waitForKeyRelease
//
// we can enter a function while the activating key is still pressed, in the new
// context the key can have a different purpose, so lets wait until it is released
// before reading it as pressed in the new context
// 
//////////////////////////////////////////////////////////////////////////////////
void waitForKeyRelease()
{
  do
  {
    // do nothing
  }
  while(getKeys() != KEY_NONE);

  // debounce
  delay(20);
}

//////////////////////////////////////////////////////////////////////////////////
//
// waitForKeyPress
//
// convenience function, loop doing nothing until one of the sKeyMask keys is 
// pressed
// 
//////////////////////////////////////////////////////////////////////////////////
uint8_t waitForKeyPress(uint8_t sKeyMask)
{
  Serial.println(getRamString(PSTR("In waitForKeyPress")));
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver, enable feedback LED, take LED feedback pin from the internal boards definition
  
  uint8_t sKey = KEY_NONE;
  
  do
  {
    sKey = getKeys() & sKeyMask;    
  }
  while(sKey == KEY_NONE);
  
  digitalWrite(BUZZER_PIN,HIGH);
  delay(20);
  digitalWrite(BUZZER_PIN,LOW);
  
  return sKey;
}

//////////////////////////////////////////////////////////////////////////////////
// 
// captureLap
//
// bFlags controls who has access, the ISR or LOOP
// loop will only read values if both are set,
// capture will only update if both are not set
// ulStartPulse can be read as the start/end of a lap if its a qualifying pulse
// 
//
//////////////////////////////////////////////////////////////////////////////////
void captureLap()
{
  if((PIND & B00000100) && (bIRPulseFlags == IR_PULSE_START_SET))
  {
    // high means end
    ulEndPulse = micros();
    ulPulseDuration = ulEndPulse - ulStartPulse;
    if(ulPulseDuration > MIN_PULSE_DURATION && ulPulseDuration < MAX_PULSE_DURATION)
    {
      ulNewLapStartTime = millis();
      bIRPulseFlags |= IR_PULSE_END_SET;
    }
    else
    {
      // this pulse is not good, clear flags and try for a good one
      bIRPulseFlags = 0;
    }
  }
  else if(!(PIND & B00000100) && (bIRPulseFlags == 0))
  {
    // low means start
    ulStartPulse = micros();
    bIRPulseFlags |= IR_PULSE_START_SET;
  }
}

void captureLap_new()
{
      ulNewLapStartTime = millis();
      bIRPulseFlags |= IR_PULSE_START_SET;
      bIRPulseFlags |= IR_PULSE_END_SET;
}

void captureLap_newx()
{
  if(bIRPulseFlags == IR_PULSE_START_SET)
  {
    // high means end
    ulEndPulse = millis();
    ulPulseDuration = ulEndPulse - ulStartPulse;
    if(ulPulseDuration > MIN_PULSE_DURATION && ulPulseDuration < MAX_PULSE_DURATION)
    {
      ulNewLapStartTime = millis();
      bIRPulseFlags |= IR_PULSE_END_SET;
    }
    else
    {
      // this pulse is not good, clear flags and try for a good one
      bIRPulseFlags = 0;
    }
  }
  else if(bIRPulseFlags == 0)
  {
    // low means start
    ulStartPulse = millis();
    bIRPulseFlags |= IR_PULSE_START_SET;
  }
}
