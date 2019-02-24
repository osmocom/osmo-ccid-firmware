#include "atmel_start_pins.h"
#include "i2c_bitbang.h"

/* FIXME: This somehow ends up with measured 125 kHz SCL speed ?!?  We should probably
 * switch away from using delay_us() and instead use some hardware timer? */
#define I2C_DELAY_US	1

#ifndef SDA1
/* We should define those pins in Atmel START. Until they are, define them here */
#define SDA1 GPIO(GPIO_PORTB, 15)
#define SCL1 GPIO(GPIO_PORTB, 14)
#define SDA2 GPIO(GPIO_PORTB, 3)
#define SCL2 GPIO(GPIO_PORTB, 2)
#define SDA3 GPIO(GPIO_PORTB, 7)
#define SCL3 GPIO(GPIO_PORTB, 6)
#define SDA4 GPIO(GPIO_PORTC, 28)
#define SCL4 GPIO(GPIO_PORTC, 27)
#endif

/* Unfortunately the schematics count I2C busses from '1', not from '0' :(
 * In software, we [obviously] count from '0' upwards. */
const struct i2c_adapter i2c[4] = {
	[0] = {
		.pin_sda = SDA1,
		.pin_scl = SCL1,
		.udelay = I2C_DELAY_US,
	},
	[1] = {
		.pin_sda = SDA2,
		.pin_scl = SCL2,
		.udelay = I2C_DELAY_US,
	},
	[2] = {
		.pin_sda = SDA3,
		.pin_scl = SCL3,
		.udelay = I2C_DELAY_US,
	},
	[3] = {
		.pin_sda = SDA4,
		.pin_scl = SCL4,
		.udelay = I2C_DELAY_US,
	}
};
