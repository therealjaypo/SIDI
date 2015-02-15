/********************************************************
 *          ______                                      *
 *    _____|\     \       ____                 _____    * 
 *   /     / |     |  ____\_  \__         _____\    \   *
 *  |      |/     /| /     /     \       /    / \    |  *
 *  |      |\____/ |/     /\      |     |    |  /___/|  *
 *  |\     \    | /|     |  |     |  ____\    \ |   ||  *
 *  | \     \___|/ |     |  |     | /    /\    \|___|/  *
 *  |  \     \     |     | /     /||    |/ \    \       *
 *   \  \_____\    |\     \_____/ ||\____\ /____/|      *
 *    \ |     |    | \_____\   | / | |   ||    | |      *
 *     \|_____|     \ |    |___|/   \|___||____|/       *
 *                   \|____|                            *
 *                                                      *
 ********************************************************
 *            SIDI: THE MIDI SID INTERFACE              *
 *            (c)2013-2015 Phar Out Studios             *
 *------------------------------------------------------*
 * This is SIDI's main Arduino sketch. It handles       *
 * including and configuring everything we're going to  *
 * need to drive the chip and receives MIDI events over *
 * the hardware serial port and converts them in to     *
 * things that the SID chip understands.                *
 *------------------------------------------------------*
 * This file is part of SIDI.                           *
 *                                                      *
 * SIDI is free software: you can redistribute it       *
 * and/or modify it under the terms of the GNU General  *
 * Public License as published by the Free Software     *
 * Foundation, either version 3 of the License, or      *
 * (at your option) any later version.                  *
 *                                                      *
 * SIDI is distributed in the hope that it will be      *
 * useful, but WITHOUT ANY WARRANTY; without even the   *
 * implied warranty of MERCHANTABILITY or FITNESS FOR   *
 * A PARTICULAR PURPOSE. See the GNU General Public     *
 * License for more details.                            *
 *                                                      *
 * You should have received a copy of the GNU General   *
 * Public License along with SIDI.                      *
 *                                                      *
 * If not, see <http://www.gnu.org/licenses/>.          *
 ********************************************************/

#include <Arduino.h>

#include "SIDINotes.h"
#include "SID6581.h"

/** Current note for all 3 channels */
unsigned long curNote[3] = { 0, 0, 0 };

/** Arpeggio data struture. Currently broken. */
byte arpeg[3][7] = {
  { 0, -1, -1, -1, -1, -1, -1 },
  { 0, -1, -1, -1, -1, -1, -1 },
  { 0, -1, -1, -1, -1, -1, -1 }
};

/** Milliseconds between notes in an arpeggio */
byte arate = 15;

/** Timestamp of last arpeggio */
unsigned long lastarp = 0;

/** Timestamp last heartbeat sent (disabled) */
unsigned long lastbeat = 0;

/**
 * Set up the serial port and start the SID chip's
 * clock running. This will also reset the chip.
 */
void setup() {
  Serial.begin(38400);
  SID.startClock();
  
  for( int x=0;x<3;x++ ) {
    SID.setEnvelope( x, 0, 0, 15, 0 );
    SID.updateEnvelope(x);
    
    SID.setShape( x, SID6581_MASK_SQUARE );
    SID.setPulseWidth( x, 2048 );
  }
}

/**
 * Sort the notes in the arpeggio structure 
 *
 * @param chan The channel to sort
 */
void sort_arpeggio(int chan) {
  int tmp, done = 0;
  
  while( done != 1 ) {
    done = 1;
    for( int x=2;x<5;x++ ) {
      if( arpeg[chan][x] > arpeg[chan][x+1] ) {
        tmp = arpeg[chan][x+1];
        arpeg[chan][x+1] = arpeg[chan][x];
        arpeg[chan][x] = tmp;
        done = 0;
      }
    }
  }
}

/**
 * Update the arpeggio's current note for
 * whatever channels it's needed.
 */
void update_arpeggios() {
  int x,note;
  
  for( int chan = 0; chan < 3; chan++ ) {
    if( arpeg[chan][0] == 1 && arpeg[chan][1] != -1 ) {
      while( arpeg[chan][ arpeg[chan][1] ] == -1 && arpeg[chan][1] < 6 ) {
        arpeg[chan][1]++;
      }
      
      note = arpeg[chan][arpeg[chan][1]];
      if( curNote[chan] != note ) {
        Serial.println("Update arpeg");
        SID.setFrequency( chan, sidinote[note] );
        SID.updateVoiceFrequency( chan );
        curNote[chan] = note;
      }
      
      arpeg[chan][1]++;
      if( arpeg[chan][1] == 6 )
        arpeg[chan][1] = 2;
    }
  }
}

/**
 * Spin continuously at Serial.read() until it returns something.
 * Potentially very dangerous.
 */
int forceRead() {
  int ret;
  
  do {
    ret = Serial.read();
  } while( ret == -1 );
  
  return ret;
}

/**
 * If there's data available on the serial port, read
 * it and act appropriately.
 *
 * If it's time to update the notes being played in an
 * arpeggio, do so.
 */
void loop() {
  int evt, chan, note, vel, foo;
  
  while( Serial.available() > 0 ) {
    evt = forceRead();
    
    chan = evt & 0x0f;
    evt &= 0xf0;
    
    switch( evt & 0xf0 ) {
      case 0x80:
      case 0x90:
        note = forceRead();
        vel = forceRead();
        
        if( arpeg[chan][0] == 1 ) {
          if( evt == 0x90 && vel != 0 ) {
            for(vel=5;vel>1;vel--) {
              if( arpeg[chan][vel] == -1 ) {
                arpeg[chan][vel] = note;
                break;
              }
            }
            
            if( vel != 1 ) {
              sort_arpeggio(chan);
              
              if( vel == 5 ) {
                arpeg[chan][1] = 2;
                curNote[chan] = note;
                SID.setFrequency(chan,sidinote[note]);
                SID.updateVoiceFrequency(chan);
                SID.voiceOn(chan);
              }
            }
          } else {
            for(vel=5;vel>1;vel--) {
              if( arpeg[chan][vel] == note ) {
                break;
              }
            }
            
            if( vel != 1 ) {
              arpeg[chan][vel] = -1;
              sort_arpeggio(chan);
              
              if( vel == 5 ) {
                arpeg[chan][1] = -1;
                curNote[chan] = 0;
                SID.voiceOff(chan);
              }
            }
          }
        } else if( chan < 3 && sidinote[note] != 0 ) {
          if( evt == 0x80 || vel == 0 ) {
            if( curNote[chan] == note ) {
              curNote[chan] = 0;
              SID.voiceOff(chan);
              curNote[chan] = 0;
            }
          } else {
            if( curNote[chan] != note ) {
              SID.setFrequency( chan, sidinote[note] );
              SID.updateVoiceFrequency( chan );
              SID.voiceOn(chan);
              curNote[chan] = note;
            }
          }
        }
        break;
        
      case 0xA0:
      case 0xB0:
        forceRead();
        forceRead();
        break;
        
      case 0xC0:
        forceRead();
        break;
        
      case 0xD0:
        forceRead();
        break;
        
      case 0xE0:
        forceRead();
        forceRead();
        break;
        
      case 0xF0:
        chan = forceRead();
        
        if( chan == 0x7D ) {
          chan = forceRead();
          
          switch( chan ) {
            // Stop a or all voices
            case 0:
              chan = forceRead();
              
              if( chan == 4 ) {
                for( chan=0;chan<3;chan++ ) {
                  SID.voiceOff(chan);
                  curNote[chan] = 0;
                  arpeg[chan][1] = -1;
                  for( evt=2;evt<6;evt++ )
                    arpeg[chan][evt] = -1;
                    
                }
              } else {
                SID.voiceOff(chan);
                curNote[chan] = 0;
                arpeg[chan][1] = -1;
                for( evt=2;evt<6;evt++ )
                    arpeg[chan][evt] = -1;
              }
              break;
              
            // Update ADSR envelope
            case 1:
              chan = forceRead();
              
              evt = forceRead();
              note = forceRead();
              vel = forceRead();
              foo = forceRead();

              SID.setEnvelope( chan, evt, note, vel, foo );
              SID.updateEnvelope(chan);
              
              break;
              
            // Update pulse width
            case 2:
              chan = forceRead();
              evt = forceRead();
              note = forceRead();
              vel = forceRead();
              
              SID.setPulseWidth( chan, (evt << 8) | (note << 4) | vel );
              
              break;
             
            // Update shape 
            case 3:
              chan = forceRead();
              evt = forceRead();
              
              SID.setShape(chan,evt);
              break;
              
           // Set test
           case 4:
             chan = forceRead();
             forceRead();
             break;
             
          // Set sync
          case 5:
            chan = forceRead();
            evt = forceRead();

            SID.setSync(chan,evt);
            break;
            
          // Update ring
          case 6:
            chan = forceRead();
            evt = forceRead();
            
            SID.setRing(chan,evt);
            break;
            
          case 7:
            chan = forceRead();
            evt = forceRead();
            
            if( evt ) {
              SID.voiceOn(chan);
            } else {
              SID.voiceOff(chan);              
            }
            break;
            
          // Volume
          case 8:
            chan = forceRead();
            SID.setVolume(chan);
            break;
            
          }
          do {
            evt = Serial.read();
          } while( evt != 0xF7 );
        } else {
          do {
            evt = Serial.read();
          } while( evt != 0xF7 );
        }
        break;
     
      // Arpeggio
      case 9:
        chan = forceRead();
        evt = forceRead();
        
        arpeg[chan][0] = evt;
        
        if( evt == 0 ) {
          arpeg[chan][1] = 2;
          for( int pos=2;pos<6;pos++ ) {
            arpeg[chan][pos] = -1;
          }
        } else
          curNote[chan] = 0;
        break;
        
      default:
        break;
    }
  }
  
  // ARPEGGIOS
  if( millis() - lastarp > arate ) {
    update_arpeggios();
    lastarp = millis();
  }

}

