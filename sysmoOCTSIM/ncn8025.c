/* Access functions for the per-SIM-slot NCN8025 chip card interface,
 * which is controlled via (half) a SX1503 I2C GPIO expander.
 *
 * (C) 2019 by Harald Welte <laforge@gnumonks.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <utils_assert.h>
#include <utils.h>
#include "atmel_start_pins.h"
#include "octsim_i2c.h"
#include "ncn8025.h"

#define SX1503_ADDR	0x20

/*! translate from ncn8025_settings into SX1503 register value */
static uint8_t ncn8025_encode(const struct ncn8025_settings *set)
{
	uint8_t reg = 0;
	if (!set->rstin)
		reg |= 0x01;
	if (!set->cmdvcc)
		reg |= 0x02;
	if (set->clkdiv & 1)
		reg |= 0x04;
	if (set->clkdiv & 2)
		reg |= 0x08;
	if (set->vsel & 1)
		reg |= 0x10;
	if (set->vsel & 2)
		reg |= 0x20;
	if (set->led)
		reg |= 0x80;
	return reg;
}

/*! translate from register value to ncn8025_settings */
static int ncn8025_decode(uint8_t reg, struct ncn8025_settings *set)
{
	memset(set, 0, sizeof(*set));

	if (!(reg & 0x01))
		set->rstin = true;
	if (!(reg & 0x02))
		set->cmdvcc = true;
	if (reg & 0x04)
		set->clkdiv |= 0x01;
	if (reg & 0x08)
		set->clkdiv |= 0x02;
	if (reg & 0x10)
		set->vsel |= 0x01;
	if (reg & 0x20)
		set->vsel |= 0x02;
	if (!(reg & 0x40))
		set->simpres = true;
	if ((reg & 0x80))
		set->led = true;

	return 0;
}

static const struct i2c_adapter *slot2adapter(unsigned int slot)
{
	unsigned int idx = slot / 2;
	ASSERT(idx < ARRAY_SIZE(i2c));
	return &i2c[idx];
}


static const uint8_t slot2data_reg(unsigned int slot)
{
	if (slot & 1)
		return 0x00;
	else
		return 0x01;
}

static const uint8_t slot2dir_reg(unsigned int slot)
{
	if (slot & 1)
		return 0x02;
	else
		return 0x03;
}

static const uint8_t slot2int_pin(unsigned int slot)
{
	static const uint8_t slot2pin[8] = { SIM0_INT, SIM1_INT, SIM2_INT, SIM3_INT,
					     SIM4_INT, SIM5_INT, SIM6_INT, SIM7_INT };
	ASSERT(slot < ARRAY_SIZE(slot2pin));
	return slot2pin[slot];
}

bool ncn8025_interrupt_level(uint8_t slot)
{
	uint8_t pin = slot2int_pin(slot);
	return gpio_get_pin_level(pin);
}


/*! Set a given NCN8025 as described in 'set'.
 *  \param[in] slot Slot number (0..7)
 *  \param[in] set Settings that shall be written
 *  \returns 0 on success; negative on error */
int ncn8025_set(uint8_t slot, const struct ncn8025_settings *set)
{
	const struct i2c_adapter *adap = slot2adapter(slot);
	uint8_t reg = slot2data_reg(slot);
	uint8_t raw = ncn8025_encode(set);
	return i2c_write_reg(adap, SX1503_ADDR, reg, raw);
}

/*! Get a given NCN8025 state from the chip.
 *  \param[in] slot Slot number (0..7)
 *  \param[out] set Settings that are retrieved
 *  \returns 0 on success; negative on error */
int ncn8025_get(uint8_t slot, struct ncn8025_settings *set)
{
	const struct i2c_adapter *adap = slot2adapter(slot);
	uint8_t reg = slot2data_reg(slot);
	int rc;
	rc = i2c_read_reg(adap, SX1503_ADDR, reg);
	if (rc < 0)
		return rc;
	rc = ncn8025_decode(rc, set);
	set->interrupt = ncn8025_interrupt_level(slot);
	return rc;
}

/*! default settings we use at start-up: powered off, in reset, slowest clock, 3V */
static const struct ncn8025_settings def_settings = {
	.rstin = true,
	.cmdvcc = false,
	.led = false,
	.clkdiv = SIM_CLKDIV_8,
	.vsel = SIM_VOLT_3V0,
};

/*! Initialize a given NCN8025/slot. */
int ncn8025_init(unsigned int slot)
{
	const struct i2c_adapter *adap = slot2adapter(slot);
	uint8_t reg = slot2dir_reg(slot);
	int rc;
	/* IO6 of each bank is input (!PRESENT), rest are outputs */
	rc = i2c_write_reg(adap, SX1503_ADDR, reg, 0x40);
	if (rc < 0)
		return rc;
	return ncn8025_set(slot, &def_settings);
}

static const char *volt_str[] = {
	[SIM_VOLT_3V0] = "3.0",
	[SIM_VOLT_5V0] = "5.0",
	[SIM_VOLT_1V8] = "1.8",
};

static const unsigned int div_val[] = {
	[SIM_CLKDIV_1] = 1,
	[SIM_CLKDIV_2] = 2,
	[SIM_CLKDIV_4] = 4,
	[SIM_CLKDIV_8] = 8,
};

void ncn8025_dump(const struct ncn8025_settings *set)
{
	printf("VOLT=%s, CLKDIV=%u", volt_str[set->vsel], div_val[set->clkdiv]);
	if (set->rstin)
		printf(", RST");
	if (set->cmdvcc)
		printf(", VCC");
	if (set->interrupt)
		printf(", INT");
	if (set->simpres)
		printf(", SIMPRES");
	if (set->led)
		printf(", LED");
}
