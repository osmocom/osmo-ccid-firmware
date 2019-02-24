/* Bit-banging I2C layer, inspired to a large extent from Linux kernel
 * i2c-algo-bit.c code (C) 1995-2000 Simon G. Vogl.  This particular
 * implementation is (C) 2019 by Harald Welte <laforge@gnumonks.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <hal_gpio.h>
#include <hal_delay.h>
#include <err_codes.h>
#include "i2c_bitbang.h"

#define setsda(adap, val)	gpio_set_pin_level((adap)->pin_sda, val)
#define setscl(adap, val)	gpio_set_pin_level((adap)->pin_scl, val)

static int getsda(const struct i2c_adapter *adap)
{
	int rc;
	gpio_set_pin_direction(adap->pin_sda, GPIO_DIRECTION_IN);
	rc = gpio_get_pin_level(adap->pin_sda);
	gpio_set_pin_direction(adap->pin_sda, GPIO_DIRECTION_OUT);
	return rc;
}

static int getscl(const struct i2c_adapter *adap)
{
	int rc;
	gpio_set_pin_direction(adap->pin_scl, GPIO_DIRECTION_IN);
	rc = gpio_get_pin_level(adap->pin_scl);
	gpio_set_pin_direction(adap->pin_scl, GPIO_DIRECTION_OUT);
	return rc;
}

static inline void sdalo(const struct i2c_adapter *adap)
{
	setsda(adap, 0);
	delay_us((adap->udelay+1) / 2);
}

static inline void sdahi(const struct i2c_adapter *adap)
{
	setsda(adap, 1);
	delay_us((adap->udelay+1) / 2);
}

static inline void scllo(const struct i2c_adapter *adap)
{
	setscl(adap, 0);
	delay_us(adap->udelay / 2);
}


static int sclhi(const struct i2c_adapter *adap)
{
	setscl(adap, 1);

	/* wait for slow slaves' clock stretching to complete */
	while (!getscl(adap)) {
		/* FIXME: abort at some point */
	}
	return 0;
}

static void i2c_start(const struct i2c_adapter *adap)
{
	/* Assert: SCL + SDA are high */
	setsda(adap, 0);		/* set SDA to low */
	delay_us(adap->udelay);		/* delay */
	scllo(adap);			/* Set SCL to low */
}

static void i2c_repstart(const struct i2c_adapter *adap)
{
	/* Assert: SCL is low */
	sdahi(adap);
	sclhi(adap);
	setsda(adap, 0);
	delay_us(adap->udelay);
	scllo(adap);
}

static void i2c_stop(const struct i2c_adapter *adap)
{
	/* Assert: SCL is low */
	sdalo(adap);			/* set SDA low */
	sclhi(adap);			/* set SCL to high */
	setsda(adap, 1);		/* set SDA to high */
	delay_us(adap->udelay);		/* delay */
}

static int i2c_outb(const struct i2c_adapter *adap, uint8_t outdata)
{
	uint8_t sb;
	int ack, i;

	/* Assert: SCL is low */
	for (i = 7; i >= 0; i--) {
		sb = (outdata >> i) & 1;
		setsda(adap, sb);
		delay_us((adap->udelay + 1) / 2);
		if (sclhi(adap) < 0)
			return -ERR_TIMEOUT;
		scllo(adap);
	}
	sdahi(adap);
	if (sclhi(adap) < 0)
		return -ERR_TIMEOUT;
	ack = !getsda(adap);
	scllo(adap);
	return ack;
}

static int i2c_inb(const struct i2c_adapter *adap)
{
	uint8_t indata = 0;
	int i;

	/* Assert: CSL is low */
	sdahi(adap);
	for (i = 0; i < 8; i++) {
		/* SCL high */
		if (sclhi(adap) < 0)
			return -ERR_TIMEOUT;
		indata = indata << 1;
		if (getsda(adap))
			indata |= 0x01;
		setscl(adap, 0);
		if (i == 7)
			delay_us(adap->udelay / 2);
		else
			delay_us(adap->udelay);
	}
	/* Assert: SCL is low */
	return indata;
}

/*! Single-byte register write. Assumes 8bit register address + 8bit values */
int i2c_write_reg(const struct i2c_adapter *adap, uint8_t addr, uint8_t reg, uint8_t val)
{
	int rc;

	i2c_start(adap);
	rc = i2c_outb(adap, addr << 1);
	if (rc < 0)
		goto out_stop;
	rc = i2c_outb(adap, reg);
	if (rc < 0)
		goto out_stop;
	rc = i2c_outb(adap, val);
out_stop:
	i2c_stop(adap);
	return rc;
}

/*! Single-byte register read. Assumes 8bit register address + 8bit values */
int i2c_read_reg(const struct i2c_adapter *adap, uint8_t addr, uint8_t reg)
{
	int rc;

	i2c_start(adap);
	rc = i2c_outb(adap, addr << 1);
	if (rc < 0)
		goto out_stop;
	rc = i2c_outb(adap, reg);
	if (rc < 0)
		goto out_stop;
	i2c_repstart(adap);
	rc = i2c_outb(adap, addr << 1 | 1);
	if (rc < 0)
		goto out_stop;
	rc = i2c_inb(adap);
out_stop:
	i2c_stop(adap);
	return rc;
}

/*! Initialize a given I2C adapter/bus */
int i2c_init(const struct i2c_adapter *adap)
{
	gpio_set_pin_direction(adap->pin_sda, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(adap->pin_scl, GPIO_DIRECTION_OUT);

	/* Bring bus to a known state. Looks like STOP if bus is not free yet */
	setscl(adap, 1);
	delay_us(adap->udelay);
	setsda(adap, 1);

	return 0;
}
