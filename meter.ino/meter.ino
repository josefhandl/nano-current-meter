#include <avr/io.h>
#include <avr/interrupt.h>

// http://www.technoblogy.com/show?2S67

// ATmega328p
//----------------
// 28-pin SOIC, 8-bit, 32kB Flash, 2kB SRAM, 1kB EEPROM, 20MHz (8MHz used)
//
//                 +----+
//           PC6  1|O   |28  PC5 ADC5
//           PD0  2|    |27  PC4 ADC4
//           PD1  3|    |26  PC3 ADC3
//           PD2  4|    |25  PC2 ADC2
//           PD3  5|    |24  PC1 ADC1
//           PD4  6|    |23  PC0 ADC0
//           VCC  7|    |22  GND
//           GND  8|    |21  AREF
//           PB6  9|    |20  VCC
//           PB7 10|    |19  PB5
//           PD5 11|    |18  PB4
//           PD6 12|    |17  PB3
//           PD7 13|    |16  PB2
//           PB0 14|    |15  PB1
//                 +----+

// PB7
// |PB6
// ||PB5
// |||PB4
// ||||PB3
// |||||PB2
// ||||||PB1
// |||||||PB0
// ||||||||
// |ABCDEFG
// DP (decimal point)

// Seven-segment definitions
const int charArrayLen = 17;
char charArray[] = {
//  PABCDEFG  Segments
  0b01111110, // 0
  0b00110000, // 1
  0b01101101, // 2
  0b01111001, // 3
  0b00110011, // 4
  0b01011011, // 5
  0b01011111, // 6
  0b01110000, // 7
  0b01111111, // 8
  0b01111011, // 9
  0b00000000, // 10  Space
  0b00000001, // 11  '-'
  0b00001110, // 12  'L'
  0b00011101, // 13  'o'
  0b00110111, // 14  'H'
  0b00000100, // 15  'i'
  0b00011100, // 16  'u'
  0b00010101, // 17  'n'
  0b10000000  // 18  '.'
};

const int symbol_Space = 10;
const int symbol_Dash = 11;
const int symbol_Lo = 12;
const int symbol_Hi = 14;
const int symbol_uA = 16;
const int symbol_nA = 17;
const int symbol_point = 18;
const int symbol_measurement = 18;
const int ndigits = 4;
volatile int Buffer[] = {symbol_Dash, symbol_Dash, symbol_Dash, symbol_Dash};
char dp = 4;  // Decimal point position 4 (off) or 0 to 1
int digit = 0;
bool measurement; // Last decimal point on 4th segment. Indicate an ongoing measurement
int measurementPin = PIN_PC0;

// Magic number for 1uF (foil) capacitor
//  magicNumber = (5 x log(2) x 10^(-6)) / 2
//                 |            \______/
//                 Voltage      Capacitance in Farads
int magicNumber = 1732868;

// Display multiplexer **********************************************
void DisplayNextDigit() {
    PORTB = ~0x00;       // Reset all 8 segments (low active)
    PORTD &= 0b00001111; // Reset left 4 segment multiplexers (high active)

    char segs = charArray[Buffer[digit]];
    // Add decimal point
    if (dp == digit) {
        segs |= charArray[symbol_point];
    }

    // Add measurement symbol
    if (measurement && 3 == digit) {
        segs |= charArray[symbol_measurement];
    }

    PORTB &= ~segs;                // Set all 8 segments (low active)
    PORTD |= (1 << (7-digit));     // Set left 4 segment multiplexers (high active)
    digit = (digit + 1) % ndigits; // Increment currently processing segment
}

// Display a 4-digit decimal number
void Display(int i) {
    for (int d = ndigits-1-1; d >= 0; --d) {
        Buffer[d] = i % 10;
        i = i / 10;
    }
}


// Set up Timer/Counter to multiplex the display
void SetupDisplay() {
    TCCR1A = 0<<WGM10;
    TCCR1B = 1<<WGM12 | 2<<CS10;  // Divide by 8
    OCR1A = 4999;                 // Compare match at 200Hz
    TIMSK1 = 1<<OCIE1A;           // Enable compare match interrupt
}

// Timer interrupt - multiplexes display
ISR(TIMER1_COMPA_vect) {
    DisplayNextDigit();
}

void ClearBuffer() {
    Buffer[0] = symbol_Space;
    Buffer[1] = symbol_Space;
    Buffer[2] = symbol_Space;
    Buffer[3] = symbol_Space;
    dp = 4;
}

void setup() {
    // Configure pins for seven-segment display
    DDRB = 0xFF; // Configure PB0-PA7 as outputs (8 segments (include decimal point))
    DDRD |= (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7); // PD4 - PD 7 as output (4 digits)

    SetupDisplay();
    sei(); // Enable global interrupts
}

void loop() {
    // Charging
    //------------------------------
    pinMode(measurementPin, OUTPUT);
    digitalWrite(measurementPin, HIGH);

    delay(700);

    // Discharging and measuring
    //------------------------------
    measurement = true;
    pinMode(measurementPin, INPUT);

    unsigned long Start = millis(), Time, nA;
    unsigned int Initial = analogRead(measurementPin);
    unsigned int Target = (Initial * 29) / 41;
    do {
        Time = millis() - Start;
    } while (analogRead(measurementPin) > Target && Time < 100000);
    measurement = false;

    ClearBuffer();

    nA = magicNumber / Time;
    if (Time >= 100000) {
        Buffer[2] = symbol_Lo;
        Buffer[3] = symbol_Lo + 1;
    } else if (nA < 1000) {
        Display(nA);
        Buffer[3] = symbol_nA;
    } else if (nA < 10000) {
        dp = 0;
        Display(nA / 10);
        Buffer[3] = symbol_uA;
    } else if (nA < 100000) {
        dp = 1;
        Display(nA / 100);
        Buffer[3] = symbol_uA;
    } else {
        Buffer[2] = symbol_Hi;
        Buffer[3] = symbol_Hi + 1;
    }
}
