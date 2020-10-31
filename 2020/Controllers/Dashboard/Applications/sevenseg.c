#include "drv_i2c.h"
#include "sevenseg.h"

const uint8_t SevenSegmentDigits[10] = {
	0x3F, /* 0 */
	0x06, /* 1 */
	0x5B, /* 2 */
	0x4F, /* 3 */
	0x66, /* 4 */
	0x6D, /* 5 */
	0x7D, /* 6 */
	0x07, /* 7 */
	0x7F, /* 8 */
	0x6F, /* 9 */
};

const uint8_t DisplayAddresses[5] = {
    0x25,
    0x26,
    0x26,
    0x27,
    0x27
};

void set_digit(uint8_t display_index, uint8_t digit) {
    if(digit > 9) {
        // Invalid digit
        return; 
    }
    
    uint8_t code = SevenSegmentDigits[digit];
    uint8_t address = DisplayAddresses[display_index];
    uint8_t tx = code;
    
    // even index digit displays are at 0x13
    uint8_t byte_two = display_index % 2 == 0;    
    uint8_t pointer = 0x12 + byte_two;
    
    drv_i2c_write_register(DRV_I2C_CHANNEL_EXPANDERS, address, pointer, &tx, 1);
}

void set_rpm(uint16_t rpm) {
    if(rpm > 10000) {
        // invalid rpm
        return;
    }
    
    // Write the last four displays
    for(int i = 1; i < 5; i++) {
        int digit = rpm % 10;
        rpm /= 10;
        set_digit(i, digit);
    }
}

void set_gear(uint8_t gear) {
    set_digit(0, gear);
}
