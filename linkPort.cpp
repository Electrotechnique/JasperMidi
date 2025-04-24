/*  linkPort.cpp
 *  Class for accessing the Link Port
 *  This version for JasperMIDI adapter
 *    
 *  Copyright Jason Lane 2017
 *
 *  This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License. 
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to 
 *  Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 * 
 */

#include "linkPort.h"


#define ON_TIME 2
#define OFF_TIME 16

volatile boolean _linkTriggered = false;
volatile boolean _writing = false;
volatile uint8_t _noteRead = 255;

// ISR called when Link port is triggered, but 
// only act if not writing a note to the link port
void linkISR() {
  if (!(_writing | _linkTriggered)) {
    _linkTriggered = true;
    _noteRead = ((PINB & 1)<<5) | (PIND>>3); // Read note value from pins 4-8
  }
}

// Constructor
LinkPort::LinkPort() {
  // Set Linkport pins as inputs, no pullups
  PORTB &= B11111110;
  DDRB  &= B11111110;
  PORTD &= B00000011;
  DDRD  &= B00000011;
  _writing = false;
  attachInterrupt(digitalPinToInterrupt(TRIG_PIN), linkISR, RISING);    
}

void LinkPort::update() {
  uint8_t oldNote;

  if (_linkTriggered) {
    _noteReading = true;
    _releaseTime = 0;
    oldNote = _currentNote;
    noInterrupts();
    _currentNote = _noteRead;
    _linkTriggered = false; // re-arm the interrupt
    interrupts();
  } else {
    if ((_noteReading && (_releaseTime>RELEASE_TIME))) {
      _noteReading = false;
    }
  }
  
  switch (_writingState) {
    case 0:
      // Start a pulse on the link port
      _writing = true; // Suppress note reading
      _writingTimer = 0;
      _writingState = 1;
      // Set pins 
      DDRB |= B00000001; // set output on pin 8
      PORTB |= _writingNoteH;
      DDRD |= B11111100; // set output mode for pins 2-7 
      PORTD |= _writingNoteL;
      //PORTB=(PINB & B11111110) | _writingNoteH;
      PORTD=(PIND & B00000011) | _writingNoteL; // set note + trigger
      //DDRB = DDRB | B00000001; // set PortB output
      DDRD = DDRD | B11111100; // set PortD output
      break;
    case 1:
      // on during
      if (_writingTimer>ON_TIME) {
        // finished writing the pulse
        // tristate the pins
        PORTD &= B00000011; // ensure output is 0
        DDRD  &= B00000011; // set input mode
        PORTB &= B11111110; // ensure output is 0
        DDRB  &= B11111110; // set input mode
        _writing = false; // Enable note reading
        _writingTimer = 0;
        _writingState = 2;
      }
      break;
    case 2:
      // off pulse
      if (_writingTimer>OFF_TIME) {
        if (_noteTimer<_writingTime) {
          // Start another pulse
          _writingState = 0;
        } else {
          // End of note
          _writingState = 6;
        }
      }
      break;
    case 3:
      // play a rest - i.e. do nothing until time expires
      if (_noteTimer>=_writingTime) {
        _writingState = 6;
      }
      break;
    case 6:
      // wait   
      ;
  }
}

// Note is 6bit number, output for time t
void LinkPort::write(uint8_t note, unsigned long t ) {
   if (note<=48) {
     // play a note
     _writingState = 0;
     // Make port output value - note + trigger
     _writingNoteL = ((note<<1) | B00000001) << 2; // set trigger bit
     _writingNoteH = (note>>5);
     _currentNote = note;
   } else { 
     // play a rest - note value >48
     _writingState = 3;
   }
   _writingTime = t;
   _noteTimer = 0;   
}

// Read last 
uint8_t LinkPort::read() {
  return _currentNote;
}

boolean LinkPort::isTriggered() {
  if (_linkTrig) {
    _linkTrig = false;
    return true;
  } else {
    return false;
  }
  
  return _linkTrig;
}

boolean LinkPort::isPlaying() {
  // Returns true if we're playing a note
  return _writingState!=6;
}


boolean LinkPort::isReading() {
  // Returns true if we're reading a note
  return _noteReading;
}

