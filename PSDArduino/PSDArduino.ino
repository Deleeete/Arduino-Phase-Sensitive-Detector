/* {PSDARduino
    Copyright (C) 2014  Kevin D. Schultz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.*/

/* This program is used to configure the Arduino as a function
generator and as DAQ. This sketch can be used in just this capacity,
but is intended to be the reference signal generator and the front
end of a Phase-Sensitive-Detector with the aid of Processing program
2Ch-PSD.pde

The reference signal is generated by modifiying the PWM on pin 9
of an Arduino.This signal is smoothed with an RC ckt on the pin 9.
A NPN transistor is used afterwards to drive an LED. The signal
from a LED acting as a photodiode is read in from pin 0.

To make this all work quickly, although it is not strictly necessary
We run the ADC in it's fastest mode see, https://sites.google.com/site/measuringstuff/the-arduino

See Fritzing Sketch PSD_sketch.fzz

*/
#include <avr/interrupt.h>

#define PI2 6.283185
#define AMP 127
#define OFFSET 128
#define REC_LENGTH 256
#define LENGTH 256

byte wave_I[LENGTH];
byte wave_Q[LENGTH];
volatile byte index = 0; // Points to each table entry. declare as volatile so other functions can see it.
float REF_I[LENGTH];     // in-phase bit here
float REF_Q[LENGTH];     // Out of phase bit
int signal[REC_LENGTH];  // Captures Signal Data

/* We are going to reconfigure ADC to be faster to go to regular speed
change to #define FASTADC 0 */

#define FASTADC 1
// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

void setup()
{
  // put your setup code here, to run once:
#if FASTADC
  // set prescale to 16
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);
#endif

  Serial.begin(38400); //run serial to processing as fast as we can

  // Create reference signal
  for (int i = 0; i < LENGTH; i++)
  {
    float REF_I = (AMP * cos(2 * PI2 / LENGTH * i)); //check why I need to redeclare float
    float REF_Q = (AMP * sin(2 * PI2 / LENGTH * i));
    wave_I[i] = int(REF_I + OFFSET); //add offset to make positive
    wave_Q[i] = int(REF_Q + OFFSET);
  }
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);      //this is going to be where are signal comes out
  TCCR1B = (1 << CS10);    //Timer runs at 16MHz
  TCCR1A |= (1 << COM1A1); //Sets pin low when TCNT1 = OCR1A
  TCCR1A |= (1 << WGM10);  // This and next line sets 8-bit fast PWM mode
  TCCR1B |= (1 << WGM12);

  /*Get timer 2 ready to call interupt function*/
  TCCR2A = 0;             //Control register A for timer 2
  TCCR2B = (1 << CS21);   //Slow this one down so prescaler is divided by factor of 8
  TIMSK2 = (1 << OCIE2A); //When TCNT2=OCRA2 call the ISR
  OCR2A = 50;             // Sets frequency
  sei();                  //enable interupts
}

void loop()
{
  /* After we have filled up the array signal at ADC[0] send array
     over to Processing via serial. Only the raw data is sent over
     mixing and filtering are to be done in processing.*/

  if (index >= LENGTH - 1)
  {
    int hold;
    for (int i = 0; i < LENGTH; i++)
    {      
        hold = (signal[i] / 2) * (wave_I[i] - OFFSET) / 4;
        Serial.print(hold); //send in-phase mix 
        Serial.print(',');
        hold = (signal[i] / 2) * (wave_Q[i] - OFFSET) / 4;
        Serial.println(hold); //send out-of-phase mix
    }
  }
}

ISR(TIMER2_COMPA_vect)
{ // Called when TCNT2 == OCR2A
  signal[index] = analogRead(A0);
  OCR1AL = wave_I[index++]; // Update the PWM output
  TCNT2 = 1;               // Timing to compensate for ISR run time
}
