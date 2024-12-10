#include <avr/io.h>
#include <avr/interrupt.h>


// ATtiny1614
//----------------
// 14-pin SOIC, 8-bit, 16kB Flash, 2kB SRAM, 256B EEPROM, 20MHz
//
//                 +----+
//           VDD  1|O   |14  GND
//   SS AIN4 PA4  2|    |13  PA3 AIN3 SCK EXTCLK
// VREF AIN5 PA5  3|    |12  PA2 AIN2 MISO EVOUTA
//      AIN6 PA6  4|    |11  PA1 AIN1 MOSI
//      AIN7 PA7  5|    |10  PA0 AIN0 RESET UPDI
// RxD TOSC1 PB3  6|    | 9  PB0 AIN11 SCL XDIR
// TxD TOSC2 PB2  7|    | 8  PB1 AIN10 SDA XCK
//                 +----+


// PA6
// |PA5
// ||PA4
// |||PA3
// ||||PA2
// |||||PA1
// ||||||PB3 < !!!
// |||||||
// ABCDEFG


// Seven-segment definitions
const int charArrayLen = 17;
char charArray[] = {
//  ABCDEFG  Segments
  0b1111110, // 0
  0b0110000, // 1
  0b1101101, // 2
  0b1111001, // 3
  0b0110011, // 4
  0b1011011, // 5
  0b1011111, // 6
  0b1110000, // 7
  0b1111111, // 8
  0b1111011, // 9
  0b0000000, // 10  Space
  0b0000001, // 11  '-'
  0b0001110, // 12  'L'
  0b0011101, // 13  'o'
  0b0110111, // 14  'H'
  0b0000100, // 15  'i'
  0b0011100  // 16  'u'
};

const int Space = 10;
const int Dash = 11;
const int Lo = 12;
const int Hi = 14;
const int uA = 16;
const int ndigits = 3;
volatile int Buffer[] = {Dash, Dash, Dash};
char dp = 2;  // Decimal point position 2 (off) or 0 to 1
int digit = 0;

// Display multiplexer **********************************************
void DisplayNextDigit() {
    PORTA.OUTCLR = 0x7E; // Clear all segments
    PORTB.OUTCLR = 0b1111;
    digit = (digit + 1) % ndigits;
    char segs = charArray[Buffer[digit]];
    if (dp == digit && dp != 2) {
        PORTB.DIRSET = (1 << 2); // Set PB2 as an output
    }
    PORTA.OUTSET = (~segs & 0x7E); // Set segments (active low)
    PORTB.OUTSET = (1 << digit) | ((~segs & 0b1) << 3);  // Activate current digit
}

// Display a three-digit decimal number
void Display(int i) {
    for (int d = 2; d >= 0; d--) {
        Buffer[d] = i % 10;
        i = i / 10;
    }
}


// Set up Timer/Counter to multiplex the display
void SetupDisplay() {
    TCA0.SINGLE.PER = 4999;  // Set period for 200Hz interrupt
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV8_gc | TCA_SINGLE_ENABLE_bm; // Enable timer, prescaler=8
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm; // Enable overflow interrupt
}

// Timer interrupt - multiplexes display
ISR(TCA0_OVF_vect) {
    DisplayNextDigit();
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm; // Clear interrupt flag
}


void systemReset() {
    // Disable interrupts
    cli();
    
    // Enable the Watchdog Timer system reset
    CCP = CCP_IOREG_gc;       // Unlock protected registers
    WDT.CTRLA = WDT_PERIOD_8CLK_gc; // Set WDT timeout to the shortest period (8 clock cycles)

    // Wait for the WDT to trigger a reset
    while (1) {
        // Do nothing and let the watchdog timer reset the microcontroller
    }
}


// Setup **********************************************
void setup() {
    // Configure pins for seven-segment display
    PORTA.DIRSET = 0x7E; // Configure PA1-PA6 as outputs
    PORTB.DIRSET = 0b1111; // Configure PB0-PB2 + PB3 (7-seg G) as digit select outputs

    // Init button for reset
    pinMode(PIN_PA0, INPUT);

    SetupDisplay();
    sei(); // Enable global interrupts
}

void loop() {
  //DisplayNextDigit();
  //delay(1);

    PORTA.DIRSET = PIN7_bm;  // Configure PA7 as output
    PORTA.OUTSET = PIN7_bm;  // Set PA7 high

    Buffer[0] = Dash;
    Buffer[1] = Dash;
    Buffer[2] = Dash;

    delay(500);
    PORTA.DIRCLR = PIN7_bm;  // Configure PA7 as input
    unsigned long Start = millis(), Time, nA;
    unsigned int Initial = analogRead(PIN_PA7);
    unsigned int Target = (Initial * 29) / 41;
    do {
        Time = millis() - Start;
    } while (analogRead(PIN_PA7) > Target && Time < 100000);

    // my custom correction - i don't know why - 17328680 instead of 1732868
    nA = 1732868 / Time;
    dp = 2;
    if (Time >= 100000) {
        Buffer[0] = Lo;
        Buffer[1] = Lo + 1;
        Buffer[2] = Space;
    } else if (nA < 1000) {
        dp = 2;
        Display(nA);
    } else if (nA < 10000) {
        dp = 0;
        Display(nA / 10);
        Buffer[2] = uA;
    } else if (nA < 100000) {
        dp = 2;
        Display(nA / 100);
        Buffer[2] = uA;
    } else {
        Buffer[0] = Hi;
        Buffer[1] = Hi + 1;
        Buffer[2] = Space;
    }
    PORTA.DIRSET = PIN7_bm;
    PORTA.OUTSET = PIN7_bm; // Set PA7 high
    
  while (!digitalRead(PIN_PA0)) {
    delay(10);
  }
}
