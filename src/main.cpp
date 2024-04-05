/* 
    ***SET THE FOLLOWING FUSES ONLY***
    SUT0=0
    CKSEL3=0
    CKSEL2=0
    CKSEL0=0
    RSTDISBL=0
*/

#include <Arduino.h>
#include <TinyWireS.h>

#define ALT_DATA    1       // set to 1 to use 3 byte data set

//      name        port       pin
// RESET                        1
// XPOS             PB3         2
// YPOS             PB4         3
// GND                          4
// SDA              PB0         5
#define SWITCH_PIN  PB1      // 6
// SCL              PB2         7
// VCC                          8

#define I2C_ADDRESS 0x11    // set this to anything
#define CENTRE      127     // halfway 0 to 255
#define DEADZONE    20      // point at which movement registers as valid

// volatile uint8_t positionRegister;   // byte of data to send on request event
/*  POSITION BYTE
    define this by 8 bits: 0b00000xxx
    BYTES 1 & 2: 00 = no movement; 10 = joystick left; 01 = joystick right
    BYTES 3 & 4: 00 = no movement; 10 = joystick up; 01 = joystick down
    BYTES 5: 0 = no press; 1 = button pressed
    BYTES 6 to 8: dont care
    all other bit configs: dont care / error
    ie L_FLAG | R_FLAG | U_FLAG | D_FLAG | B_FLAG | X | X | X
*/

// try LR reg, UD reg, BTN reg

volatile uint8_t i2cRegs[] = { 0xAA, 0xAA, 0xAA }; // default value to attempt error catching
volatile uint8_t regIndex = 0;
const uint8_t regSize = sizeof(i2cRegs);

void requestEvent() {    
    // send data
    TinyWireS.send(i2cRegs[regIndex]); // i dont know why this works and just sending a plain byte doesnt, but it definitely breaks most of the time when i try
    regIndex++; // increment
    if (regIndex >= regSize) {
        regIndex = 0; // reset
    }
}

void setupYAnalog() {
    ADMUX =
        (1 << ADLAR) |     // left shift result
        (0 << REFS1) |     // Sets ref. voltage to VCC, bit 1
        (0 << REFS0) |     // Sets ref. voltage to VCC, bit 0
        (0 << MUX3)  |     // use ADC2 for input (PB4), MUX bit 3
        (0 << MUX2)  |     // use ADC2 for input (PB4), MUX bit 2
        (1 << MUX1)  |     // use ADC2 for input (PB4), MUX bit 1
        (0 << MUX0);       // use ADC2 for input (PB4), MUX bit 0
}

void setupXAnalog() {
    ADMUX =
        (1 << ADLAR) |     // left shift result
        (0 << REFS1) |     // Sets ref. voltage to VCC, bit 1
        (0 << REFS0) |     // Sets ref. voltage to VCC, bit 0
        (0 << MUX3)  |     // use ADC3 for input (PB3), MUX bit 3
        (0 << MUX2)  |     // use ADC3 for input (PB3), MUX bit 2
        (1 << MUX1)  |     // use ADC3 for input (PB3), MUX bit 1
        (1 << MUX0);       // use ADC3 for input (PB3), MUX bit 0
}

void singleByteData(uint8_t xPosition, uint8_t yPosition, uint8_t switchState){
    // we dont actually care about 'regIndex' here but to same elborate programming, write to whichever one is active
    if (switchState) {
        i2cRegs[regIndex] |= (0b1 << 3); // 0bxxxx1xxx
    } else {
        i2cRegs[regIndex] &= ~(0b1 << 3); // 0bxxxx0xxx
    }

    if (xPosition < (CENTRE - DEADZONE)) {
        // left
        i2cRegs[regIndex] &= ~(0b11 << 6); // 0b00xxxxxx 
        i2cRegs[regIndex] |= (0b10 << 6); // 0b01xxxxx
    } else if (xPosition > (CENTRE + DEADZONE)) {
        // right
        i2cRegs[regIndex] &= ~(0b11 << 6); // 0b00xxxxxx 
        i2cRegs[regIndex] |= (0b01 << 6); // 0b10xxxxxx
    } else {
        // centered
        i2cRegs[regIndex] &= ~(0b11 << 6); // 0b00xxxxxx
    }

    // note up / down are reversed to left / right
    if (yPosition < (CENTRE - DEADZONE)) {
        // up
        i2cRegs[regIndex] &= ~(0b11 << 4); // 0bxx00xxxx 
        i2cRegs[regIndex] |= (0b01 << 4); // 0bxx10xxxx
    } else if (yPosition > (CENTRE + DEADZONE)) {
        // down
        i2cRegs[regIndex] &= ~(0b11 << 4); // 0bxx00xxxx 
        i2cRegs[regIndex] |= (0b10 << 4); // 0bxx01xxxx
        
    } else {
        // centred
        i2cRegs[regIndex] &= ~(0b11 << 4); // 0bxx00xxxx
    }
}

void threeByteData(uint8_t xPosition, uint8_t yPosition, uint8_t switchState){
    if (switchState) {
        i2cRegs[0] |= (0xF << 0); // 0bxxxx1111
    } else {
        i2cRegs[0] &= ~(0xF << 3); // 0bxxxx0000
    }

    // keep deadzone in or jitter becomes annoying
    if ((xPosition < (CENTRE - DEADZONE)) || (xPosition > (CENTRE + DEADZONE))) {
        i2cRegs[1] = xPosition;
    }

    if ((yPosition < (CENTRE - DEADZONE)) || (yPosition > (CENTRE + DEADZONE))) {
        i2cRegs[2] = yPosition;
    }
}

void setup() {
    // ADC SETUP: set prescaler to 64 (125 kHz for 8 MHz clock)
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (0 << ADPS0); 
    
    // DIGITAL PIN SETUP
    DDRB &= ~(1 << SWITCH_PIN);  // set as input

    // I2C SETUP
    TinyWireS.begin(I2C_ADDRESS);
    TinyWireS.onRequest(requestEvent);

    i2cRegs[0] = 0x00; // reset
}

void loop() {
    TinyWireS.stateCheck(); // TinyWireS requirement

    uint8_t xPosition;
    uint8_t yPosition;
    uint8_t switchState;

    setupXAnalog();                 // set up ADC for X input
    ADCSRA |= (1 << ADSC);          // start ADC measurement
    while (ADCSRA & (1 << ADSC) );  // wait till conversion complete
    xPosition = ADCH;              // store measurement

    setupYAnalog();                 // set up ADC for Y input
    ADCSRA |= (1 << ADSC);          // start ADC measurement
    while (ADCSRA & (1 << ADSC) );  // wait till conversion complete
    yPosition = ADCH;              // store measurement
    
    switchState = ((PINB >> SWITCH_PIN) & 1); // get switch position
    
    // data processing
    if (ALT_DATA) {
        threeByteData(xPosition, yPosition, switchState)
    } else {
        singleByteDataByteData(xPosition, yPosition, switchState)
    }
    
}