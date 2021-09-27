/* 32 sample sine wave look up table.
   16Mhz CPU clk & clkI/O -> Prescaler of 2 -> 8Mhz
   1 OVF scenario will occur every 256 * 2 = 512 samples.
   1/8MHz * 512 = 64[us]
   going over the 15 sample look-up table will take 64us * 15 = 0.96[ms] -> 1,041.66[Hz]
   I'm aiming for 100[Hz] and below.
   So to achieve a 100[Hz] sine wave, I will need to increment in the sine table every
   1,041.66 / 100 = 10.41 OVF scenarios
   for 90[Hz] -> 1,041.66 / 90 = 11.57 OVF scenarios etc.
   Lower frequency resolution at higher frequencies since using byte data type (Rounded to a whole number)
   The amplitude will be attenuated by multiplying the sine table values by
   the desired frequency divided by the base frequency (60 OR 50 [Hz]) in order to maintain a constant V/Hz value.
   For 40 [Hz], the amplitude will be attenuated by 40/60 = 2/3.
   Due to low resolution of compare registers (only 1 byte), attenuating by dividing the sine table values
   will eventually lead to a distorted sine wave.
   Consider the following to overcome the distorted sine wave issue:
   Since each sine index is repeated at least 10 times (10 OVF scenarios for max freq of 100 [Hz])
   I can simply turn all the transistors off for some of the OVF scenarios, depending on the desired attenuation.
   For now, after testing, seems like even at an amplitude of 10%, the sine wave form still remains. So will continue using division for now.
   //
   REMINDER: Charge low side mosfets for at least 10[ms] at 50% duty cycle prior to normal operation (App note AN4043, P. 34)***************************
   //
   If operating a 3 phase motor, the 3 sine waves need to be 120 def apart
   If operating a single phase motor, we have 2 options depending on the wiring:
   1. With the capacitors removed, the phase shift between the main and auxiliary windings is achieved by connecting all three phases and the 3 sine waves can still be 120 degrees apart
   2. With the capacitor/s installed, A single sine wave will be used connecting only 2 phases, so the outputs need to be inverted. 2 sine waves 180 degrees phase shifted.
*/
#define _DISABLE_ARDUINO_TIMER0_INTERRUPT_HANDLER_  //These 2 lines were added to be able to compile. Also changed wiring.c file. Disables the previous overflow handles used for millis(), micros(), delay() etc.
#include <wiring.c>                                 //Reference: https://stackoverflow.com/questions/46573550/atmel-arduino-isrtimer0-ovf-vect-wont-compile-first-defined-in-vector/48779546
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#define BASE_FREQ 1552       //Hz

volatile uint8_t count = 0;
volatile uint8_t count120 = 5;
volatile uint8_t count240 = 10;
volatile uint8_t Desired_Freq = 1;
volatile uint32_t Freq_Counter = 0;
volatile float   Amp = 1.0;
const unsigned char DT = 1; //Dead time to prevent short-circuit betweem high & low mosfets
const unsigned char Sine_Len = 15;
const unsigned char Sine[] = {0x7f,0xb5,0xe1,0xfa,0xfa,0xe1,0xb5,0x7f,0x48,0x1c,0x3,0x3,0x1c,0x48,0x7f};

void setup()
{
  DDRD = (1 << PORTD6) | (1 << PORTD5) | (1 << PORTD3); //Sets the OC0A, OC0B and OC2B pins to outputs
  DDRB = (1 << PORTB3) | (1 << PORTB2) | (1 << PORTB1); //Sets the OC2A, OC1B and OC1A pins to outputs
  cli();                      //Disable interrupts
  CLKPR = (1 << CLKPCE);      //Enable change of the clock prescaler
  CLKPR = (1 << CLKPS0);      //Set system clock prescaler to 2
  //Timer 0
  TCNT0 = 0;                  //Zero counter of timer 0
  TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << COM0B0) | (1 << WGM00); // Clear OC0A and set OC0B counting up. Waveform mode 1 (Table 14-8)
  TCCR0B = (1 << CS00);       //No prescaler
  TIMSK0 = (1 << TOIE0);      //Timer/Counter0 Overflow Interrupt Enable
  OCR0A = Sine[0] - DT;   //Sign determined by set or clear at count-up
  OCR0B = Sine[0] + 2*DT;   //Sign determined by set or clear at count-up
  // Timer 1
  TCNT1 = 0;                  //Zero counter of timer 0
  TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << COM1B0) | (1 << WGM10); // Clear OC1A and set OC1B counting up. Waveform mode 1 (Table 14-8)
  TCCR1B = (1 << CS10);       //No prescaler
  OCR1A = Sine[count120] - DT;   //Sign determined by set or clear at count-up
  OCR1B = Sine[count120] + 2*DT;   //Sign determined by set or clear at count-up
  // Timer 2
  TCNT2 = 0;                  //Zero counter of timer 0
  TCCR2A = (1 << COM2A1) | (1 << COM2B1) | (1 << COM2B0) | (1 << WGM20); // Clear OC0A and set OC0B counting up. Waveform mode 1 (Table 14-8)
  TCCR2B = (1 << CS20);       //No prescaler
  OCR2A = Sine[count240] - DT;   //Sign determined by set or clear at count-up
  OCR2B = Sine[count240] + 2*DT;   //Sign determined by set or clear at count-up
  sei();
  while (1)  {}
}



ISR (TIMER0_OVF_vect)
{
  Desired_Freq = 100;
  Freq_Counter++;
  if (Freq_Counter >= (BASE_FREQ / Desired_Freq))
  {
    count++;
    count120++;
    count240++;
    if (count == Sine_Len) count = 0;
    if (count120 == Sine_Len) count120 = 0;
    if (count240 == Sine_Len) count240 = 0;    
    //  
    if ((Sine[count] - DT) < 0) OCR0A = 0;
    else  OCR0A = Sine[count] - DT;  //Sign determined by set or clear at count-up
    OCR0B = Sine[count] + 2*DT;  //Sign determined by set or clear at count-up
    //
    OCR1A = Sine[count120] - DT;  //Sign determined by set or clear at count-up
    OCR1B = Sine[count120] + 2*DT;  //Sign determined by set or clear at count-up
    OCR2A = Sine[count240] - DT;  //Sign determined by set or clear at count-up
    OCR2B = Sine[count240] + 2*DT;  //Sign determined by set or clear at count-up
    Freq_Counter = 0;
  }
}
