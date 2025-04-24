/*
      Jasper MIDI
      
      A simple MIDI interface for Jasper/Wasp or other Link enabled synths
      
      Copyright Jason Lane 2017

      This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License. 
      To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to 
      Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

      See also separate licence and copyright notices on amended libraries/header files: debugSerial.h and digitalWriteFast.h

*/

      
// Debug mode - comment out the following line for final version
//#define DEBUG 1

// Arduino Library includes
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <PinChangeInterrupt.h>
#include <EEPROM.h>
#include <elapsedMillis.h>

// Local includes
#include "pinTimer.h"
#include "linkPort.h"

#ifdef DEBUG
#include "debugSerial.h"
DebugSerial dbgSerial(17);

#define DEBUG_SERIAL_BEGIN dbgSerial.begin(57600)
#define DEBUG_PRINTLN(x) dbgSerial.println(x)
#define DEBUG_PRINT(x) dbgSerial.print(x)
#else
#define DEBUG_SERIAL_BEGIN
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#endif

#define BTNPIN A0
#define LEDPIN A5

#define MIDI_MIN_NOTE 34
#define MIDI_MAX_NOTE 72

// LINK-MIDI-LINK Lookup tables

// MIDI note lookup table
const uint8_t noteMIDIlookup[] { 
    72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 0,0,0,0,
    60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 0,0,0,0,
    48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 0,0,0,0,
    36, 35, 34 };
//  C   B   A#  A   G#  G   F#  F   E   D#  D   C# 

const uint8_t MIDIlinkLookup[39] = {
                                        50, 49, 48,
    43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32,
    27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
    11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0  
};
//  C#  D   D#  E   F   F#  G   G#  A   A#  B   C
// subtract by MIDI_MIN_NOTE to get index


LinkPort linkPort;

uint8_t noteOnCmd  = 0x90;
uint8_t noteOffCmd = 0x80;
uint8_t controlChangeCmd = 0xB0;
uint8_t allNotesOffCmd = 0x7B;

#define CHANNEL_ADDR 1
volatile uint8_t midiChannel = 0;
boolean setMIDIchan = false;

#define BUF_SIZE 6

elapsedMillis sleepTimer = 0;
#define SLEEPTIME 90000

struct noteStack {
  uint8_t s[BUF_SIZE];
  int8_t top = -1;
} nStack;

PinTimer led(LEDPIN);

void setup() {
  DEBUG_SERIAL_BEGIN;
  DEBUG_PRINTLN("Debug mode...");
  Serial.begin(31250); // Start MIDI on hardware serial
  pinMode(BTNPIN,INPUT_PULLUP);
  led.on();
  delay(1000);
  led.off();
  if (digitalRead(BTNPIN)) {
    midiChannel = readMIDIchannel();
  } else {
    // reset channel to 0
    midiChannel = 0;
    writeMIDIchannel( midiChannel );
  }
  // Send all notes off on MIDI out
  sendAllNotesOff();
}

void loop() {
  // Service routines
  led.update();
  linkPort.update();
  if (Serial.available()) {
    sleepTimer = 0;
    if (!setMIDIchan) {
      parseMIDI();
    } else {
      uint8_t m=Serial.read();
      if (m>=0x80 && m<=0xEF) {
        // get channel from command byte
        midiChannel = m & 0x0F;
        noteOnCmd  = 0x90|midiChannel;
        noteOffCmd = 0x80|midiChannel;
        controlChangeCmd = 0xB0|midiChannel;
        EEPROM.write(CHANNEL_ADDR,midiChannel);
        setMIDIchan = false;
        led.off();
        led.oscillate(50,20); // fast flash to indicate channel set
      }
    }
  }
  if (nStack.top!=-1) {
    // Playing a note from MIDI
    if (!linkPort.isPlaying()) {
      // Play the link port
      linkPort.write( MIDIlinkLookup[ nStack.s[nStack.top]-MIDI_MIN_NOTE],30);
      sleepTimer = 0;
    }
  } 
  handleMIDIout();
  handleButton();
  // Go to sleep if gate not used for a while
  if (sleepTimer > SLEEPTIME) gotoSleep();
}

void wakeISR() {
  ; // just wake up
}

void gotoSleep() {
  // attach the new PinChangeInterrupt and enable event function below
   set_sleep_mode( SLEEP_MODE_PWR_DOWN );
   noInterrupts();
   sleep_enable();
   attachPCINT(digitalPinToPCINT(0), wakeISR, CHANGE);
   attachPCINT(digitalPinToPCINT(BTNPIN), wakeISR, CHANGE); 
   // Go to sleep
   interrupts();
   sleep_cpu();
   // and wake up
   sleep_disable();
   detachPCINT(digitalPinToPCINT(0));
   detachPCINT(digitalPinToPCINT(BTNPIN)); 
   sleepTimer = 0;
}  

uint8_t readMIDIchannel() {
  uint8_t chan = EEPROM.read(CHANNEL_ADDR);
  if (chan>=16) {
    chan=0;
  }
  noteOnCmd  = 0x90|chan;
  noteOffCmd = 0x80|chan;
  controlChangeCmd = 0xB0|midiChannel;
  return chan;
}

void writeMIDIchannel(uint8_t chan) {
  // Store the MIDI channel
  EEPROM.write(CHANNEL_ADDR,chan);
}

void sendNoteOn( uint8_t note) {
    Serial.write(noteOnCmd);
    Serial.write(note);
    Serial.write(127);
}

void sendNoteOff( uint8_t note) {
    Serial.write(noteOffCmd);
    Serial.write(note);
    Serial.write(0);
}

void sendAllNotesOff() {
    Serial.write(controlChangeCmd);
    Serial.write(allNotesOffCmd);
    Serial.write(0);
}

/* Keep a stack of currently active notes 
 *  Notes are pushed onto the end of the stack
 *  If the buffer is full, the oldest note is lost.
 *  A note can be removed from the middle of the stack
 *  which is moved up.
 *  The topmost note is played on the link port
 */



void pushNote(uint8_t note) {
  if (nStack.top==BUF_SIZE-1) {
    // At top - shuffle notes down
    for (uint8_t i=0; i<nStack.top; i++) nStack.s[i]=nStack.s[i+1];
  } else {
    nStack.top++;
  }
  nStack.s[nStack.top]=note;
}

void removeNote(uint8_t note) {
  // Find note in stack
  uint8_t i = 0;  
  for (; i < BUF_SIZE && nStack.s[i] != note; i++);
  if (i == BUF_SIZE) return; // Not found
  for (uint8_t j=i; j<nStack.top; j++) nStack.s[j]=nStack.s[j+1];
  nStack.s[nStack.top] = 0;
  nStack.top--;
}

void clearNotes() {
  // Empty the note stack to stop playing
  nStack.top = -1;
  for (uint8_t i=0; i < BUF_SIZE; i++) nStack.s[i]=0;
}

void parseMIDI() {
  // Call when serial data is available
  static uint8_t state=0;
  static uint8_t cmd=0;
  static uint8_t note=0;
  static uint8_t vel=0;
  static uint8_t noteCount = 0;
  uint8_t m = Serial.read();
  if (m>=0x80 && m<=0xEF) {
    // Voice Category Status
    cmd = m;
    state = 0; 
  } else if (m>=0xF0 && m<=0xF7) {
    // System Common Status
    cmd = 0;
    state = 0;
  }
  if (cmd>0) {
    switch (state) {
      case 0:
        // cmd read
        if (cmd==noteOnCmd) {
          state = 1;
        } else if (cmd==noteOffCmd) {
          state = 3;
        } else if (cmd==controlChangeCmd) {
          state = 5;
        } else {
          // Not recognised command so reset
          cmd = 0;
        }
        break;
      case 1:
        // read note
        note = m;
        state=2;
        break;
      case 2:
        // read vel
        vel = m;
        state = 1;
        if (note>=MIDI_MIN_NOTE && note <=MIDI_MAX_NOTE) {
          // Only use Link range
          if (vel>0) {
            // Note on
            pushNote(note);
            noteCount++;
          } else {
            // Note off
            removeNote(note);
            if (noteCount>0) noteCount--;   
          }
        }
        #ifdef DEBUG
        dbgSerial.print("NoteCount: ");
        dbgSerial.println(noteCount,DEC);               
        #endif
      break;
      case 3:
        // Note Off read note
        note = m;
        state = 4;
      break;
      case 4:
        // Note Off read vel
        vel = m;
        removeNote(note);
        if (noteCount>0) noteCount--; 
        state = 3;
      break;
      case 5:
        // Control change
        if (m>=0x7B && m<=0x7F) {
          // Clear all notes
          noteCount = 0;
          clearNotes();
        }
        state = 0;
      break;
    }
  }
  
}


void handleMIDIout() {
  static uint8_t currentMIDInote = 0;
  uint8_t oldNote = currentMIDInote;
  // change midi note
  if (linkPort.isReading()||linkPort.isPlaying()) {
    currentMIDInote = noteMIDIlookup[linkPort.read()];
  } else {
    currentMIDInote = 0;
  } 
  if (oldNote!=currentMIDInote) {
    if (oldNote!=0) {
      sendNoteOff(oldNote);
      led.off();
    }
    if (currentMIDInote!=0) {
      DEBUG_PRINTLN(currentMIDInote);
      sendNoteOn(currentMIDInote);
      led.on();
    }
  }
}

elapsedMillis btnTimer = 0;

void handleButton() {
  // State machine to handle setting the MIDI channel
  // Unpressed, button is HIGH.
  // Hold button down for 3 secs to enter channel set mode
  static uint8_t state = 0;
  switch (state) {
    case 0:
      // Wait for button to be pressed
      if (digitalReadFast(BTNPIN)==LOW) {
        btnTimer = 0;
        state = 1;
      }
      break;
    case 1:
      // Wait for release or button held > 4 seconds
      if (digitalReadFast(BTNPIN)==HIGH) {
        state = 0;
      } else if (btnTimer>4000) {
        // start MIDI set mode
        state = 2;
        setMIDIchan = true;
        btnTimer = 0;
        led.oscillate(250);
      }
      break;
    case 2: 
      // wait for timeout 
      if (btnTimer>10000 || !setMIDIchan) {
        setMIDIchan = false;
        state = 0;
        led.off();
      }
      break;
  }
  
  
}

