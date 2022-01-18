/*
  Copyright 2022 Timo Diep

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/* MSC Fader Bank V1 1/17/2022

   NOTE: This software was made for a very specific use case where submasters 1 through 5 are assigned to things that don't change very often between cues.
         Also this will proobably not be updated and it's very very hodgepodged together.

   Uses an Arduino Leonardo to control submasters 1 through 5 and cue stop/go in ETC EOS. Tested with ETC nomad software, will probably work on consoles but untested.

   TODO:
        Add support for multiple pages of faders and 7-segment page display
        Add support for motorized faders
        Chnage function getLvlVdiv() to return values insstead of using globals
*/

#include "USB-MIDI.h"
#include "MIDI.h"
#include "stdint.h"

//Prints debug values to the arduino serial monitor. 
#define DEBUG false

//NOTE/TODO: this can be improved on, but it works right now.
#define NUMBER_OF_FADERS  5
#define NUMBER_OF_BUTTONS 9
#define OFFSET 3
#define VIN 5 // [v]
#define DEVICE_ID 0x00

//Used for page display outputs
/*
#define B0 5
#define B1 6
#define B2 7
*/

// Keys(pg change + bumps)
uint8_t key[NUMBER_OF_BUTTONS];
uint8_t keyState[NUMBER_OF_BUTTONS];
uint8_t page = 1;

// Faders(arduino + eos address)
uint8_t pot[NUMBER_OF_FADERS] = {A0, A1, A2, A3, A4};
uint8_t chan[NUMBER_OF_FADERS] = {1, 2, 3, 4, 5};
uint8_t lvl[NUMBER_OF_FADERS];
uint8_t prev[NUMBER_OF_FADERS];

//Oh boy, more hodgepodge
bool buffer[2] = {false, false};

bool bumpHold[NUMBER_OF_FADERS] = {false, false, false, false, false};
USBMIDI_CREATE_DEFAULT_INSTANCE();

/*
   Initialize serial bus and digital input pins for key input
*/
void setup() {
  MIDI.begin(0);
  Serial.begin(9600);

  for (int i = 0; i < NUMBER_OF_BUTTONS; i++) {
    pinMode(i, INPUT);
  }
  /* 7-Segment display output
  pinMode(B0 , OUTPUT);
  pinMode(B1 , OUTPUT);
  pinMode(B2 , OUTPUT);
  */
}

/*
   Main Function
   gets level of potentiomenters(faders) then sends the level to
   etc eos using midi show control
*/
void loop() {
  for (int i = 0; i < sizeof(lvl); i++) {
    prev[i] = lvl[i];
  }

  delay(10);//This really doesn't need to be here but it is anyways

  //Polls each fader for a change in level and sends if it's different
  getLvlVdiv();
  
  //Polls each key for a high or low lvl and does some other stuff
  //Not enough interrupt pins for each of them to be interrupts unfortunately
  keyCheck();

  for (int i = 0; i < NUMBER_OF_FADERS; i++) {
    //Prevents sending unessesary midi commands by checking if a bump button is being held or if the level hasn't changed
    if (!bumpHold[i] && (prev[i] != lvl[i])) { 
      uint8_t subNumber = chan[i] * page; //Here incase page control is ever added
      sendLvl(subNumber, lvl[i]);
      
      if(DEBUG){
        String p1 = "lvl ";
        String p2 = " = ";
        Serial.println(p1 + i + p2 + lvl[i]);
      }
    }
  }

}

/*
    Reads voltage of each fader and turns it into a percentage
    TODO: rewrite as a return function instead of using globals
*/
void getLvlVdiv() {
  for (int i = 0; i < NUMBER_OF_FADERS; i++) {
    int16_t Vo = analogRead(pot[i]);

    Vo = abs(Vo - 1023); //Invert value because I wired it wrong like a dumbass
    Vo =  Vo * 0.0978;   //Finds percentage

    lvl[i] = Vo;
  }
}

/*
   Polls digital channels for High/Low voltage levels and sends MSC commands as needed
   TODO: compress for readability(I'm lazy so I won't)
*/
void keyCheck() {
  if (digitalRead(0) == HIGH) {
    if (!bumpHold[4]) {
      bumpHold[4] = true;
      sendLvl(5, 100);
    }
  } else {
    if (bumpHold[4]) {
      sendLvl(5, lvl[4]);
    }
    bumpHold[4] = false;
  }

  if (digitalRead(1) == HIGH) {
    if (!bumpHold[3]) {
      bumpHold[3] = true;
      sendLvl(4, 100);
    }
  } else {
    if (bumpHold[3]) {
      sendLvl(4, lvl[3]);
    }
    bumpHold[3] = false;
  }

  if (digitalRead(2) == HIGH) {
    if (!bumpHold[2]) {
      bumpHold[2] = true;
      sendLvl(3, 100);
    }
  } else {
    if (bumpHold[2]) {
      sendLvl(3, lvl[2]);
    }
    bumpHold[2] = false;
  }

  if (digitalRead(3) == HIGH) {
    if (!bumpHold[1]) {
      bumpHold[1] = true;
      sendLvl(2, 100);
    }
  } else {
    if (bumpHold[1]) {
      sendLvl(2, lvl[1]);
    }
    bumpHold[1] = false;
  }

  if (digitalRead(4) == HIGH) {
    if (!bumpHold[0]) {
      bumpHold[0] = true;
      sendLvl(1, 100);
    }
  } else {
    if (bumpHold[0]) {
      sendLvl(1, lvl[0]);
    }
    bumpHold[0] = false;
  }


  if (digitalRead(8) == HIGH) {
    if (!buffer[0]) {
      if(DEBUG)
        Serial.println("STOP");
      sendSTOP();
      buffer[0] = true;
    }
  } else {
    buffer[0] = false;
  }

  if (digitalRead(9) == HIGH) {
    if (!buffer[1]) {
      if(DEBUG)
        Serial.println("GO");
      sendGO();
      buffer[1] = true;
    }
  } else {
    buffer[1] = false;
  }

}
/*
   Sends set level command over usb using midi show control syntax
   Page 32 of Eos Family Show Control User Guide for CMD description

   PARAM:
   sub - submaster to be updated
   lvl - level for the submaster to be updated to
*/
void sendLvl(uint8_t sub, uint8_t lvl) {

  //uint8_t setLvl[] = {0xF0,0x7F,0x69,0x02,0x01,0x06,0x01,0x00,0x32,0x00,0x7F};
  uint8_t setLvl[] = {0x7F, DEVICE_ID, 0x02, 0x01, 0x06, sub, 0x00, lvl, 0x00};

  //sendSysEx: 3rd param needs to be set to false so the first 0x7F isn't read as a stop bit
  MIDI.sendSysEx(sizeof(setLvl), setLvl, false);
}

void sendGO() {
  uint8_t goCMD[] = {0x7F, DEVICE_ID, 0x02, 0x01, 0x01, 0x00};
  MIDI.sendSysEx(sizeof(goCMD), goCMD, false);
}

void sendSTOP() {
  uint8_t stopCMD[] = {0x7F, DEVICE_ID, 0x02, 0x01, 0x02, 0x00};

  MIDI.sendSysEx(sizeof(stopCMD), stopCMD, false);
}

// Used for updating a 7-segment display. Not currently included in circuit diagram but is included in initial prototype although it is not wired
/*
  void updateDisp(uint8_t val) {
  uint8_t msb;
  uint8_t mid;
  uint8_t lsb;
  if (val < 7) {
    msb = (val & 0x04) >> 2;
    mid = (val & 0x02) >> 1;
    lsb = (val & 0x01);

    digitalWrite(B2, msb);
    digitalWrite(B1, mid);
    digitalWrite(B0, lsb);
  }
  }
  */
