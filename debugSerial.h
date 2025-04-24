/*
 * A hacked version of the standard Arduino DebugSerial library
 * that transmits only.
 * 
 * All the receiver code has been removed.
 * Original source from: 
 * 
 * https://github.com/arduino/Arduino/blob/master/hardware/arduino/avr/libraries/DebugSerial
 * 
 * Original copyright header below:

DebugSerial.h (formerly NewSoftSerial.h) - 
Multi-instance software serial library for Arduino/Wiring
-- Interrupt-driven receive and other improvements by ladyada
   (http://ladyada.net)
-- Tuning, circular buffer, derivation from class Print/Stream,
   multi-instance support, porting to 8MHz processors,
   various optimizations, PROGMEM delay tables, inverse logic and 
   direct port writing by Mikal Hart (http://www.arduiniana.org)
-- Pin change interrupt macros by Paul Stoffregen (http://www.pjrc.com)
-- 20MHz processor support by Garrett Mace (http://www.macetech.com)
-- ATmega1280/2560 support by Brett Hagman (http://www.roguerobotics.com/)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

The latest version of this library can always be found at
http://arduiniana.org.
*/

#ifndef DebugSerial_h
#define DebugSerial_h

#include <inttypes.h>
#include <Stream.h>

/******************************************************************************
* Definitions
******************************************************************************/


#ifndef GCC_VERSION
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

class DebugSerial : public Stream
{
private:
  // per object data
  uint8_t _transmitBitMask;
  volatile uint8_t *_transmitPortRegister;
  volatile uint8_t *_pcint_maskreg;
  uint8_t _pcint_maskvalue;

  uint16_t _tx_delay;

  // static data
  static DebugSerial *active_object;

  // private methods
  void setTX(uint8_t transmitPin);
  
  // Return num - sub, or 1 if the result would be < 1
  static uint16_t subtract_cap(uint16_t num, uint16_t sub);

  // private static method for timing
  static inline void tunedDelay(uint16_t delay);

public:
  // public methods
  DebugSerial(uint8_t transmitPin);
  void begin(long speed);
  
  virtual size_t write(uint8_t byte);
  operator bool() { return true; }
  
  using Print::write;

  // Dummy read stuff
  int peek();
  virtual int read();
  virtual int available();
  virtual void flush();
  bool isListening() { return this == active_object; }
  bool stopListening();
  //bool overflow() { bool ret = _buffer_overflow; if (ret) _buffer_overflow = false; return ret; }

};

#endif

