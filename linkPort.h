/*  linkPort.h
 *  Class for accessing the Link Port
 *  This version for JasperMIDI adapter
 *
 *  Link Port is connected to pins
 *  2,3,4,5,6,7,8
 *  T,A,B,C,D,E,F
 *  
 *  Note is read when the trigger interrupt is RISING
 *  Uses hardware interrupt INT0 for the trigger on pin D2
 *  This frees up the hardware serial port for MIDI interfacing, 
 *  but means the signal F is on portB, complicating the link port access slightly.
 *     
 *  Copyright Jason Lane 2017
 *
 *  This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License. 
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to 
 *  Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 * 
 */

#ifndef linkPort_h
#define linkPort_h

#include <Arduino.h>
#include <elapsedMillis.h>
#include <avr/pgmspace.h>

#include "digitalWriteFast.h"

#define TRIG_PIN 2
#define RELEASE_TIME 25


void linkISR();

class LinkPort {
  public:
    // Constructor    
    LinkPort();
    void update();
    // Write value to link port for time t
    void write( uint8_t note, unsigned long t );
    // Read last note from link port
    uint8_t read();      // Returns the current/last note read
    boolean isPlaying(); // If linkport is being played by this library
    boolean isReading(); // If note currently being read from port
    boolean isTriggered(); // If the port was triggered       
  private:
    boolean _linkTrig = false;
    boolean _noteReading = false;
    elapsedMillis _releaseTime = 0;
    uint8_t _currentNote = 255;   
    uint8_t _writingState = 0;
    elapsedMillis _noteTimer = 0;    
    elapsedMillis _writingTimer;
    unsigned long _writingTime = 0;
    uint8_t _writingNoteH,_writingNoteL;
};

#endif

