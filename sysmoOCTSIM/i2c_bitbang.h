#pragma once
#include <stdint.h>

struct i2c_adapter {
	uint8_t	pin_scl;
	uint8_t	pin_sda;
	uint32_t udelay;
};

int i2c_init(const struct i2c_adapter *adap);
int i2c_write_reg(const struct i2c_adapter *adap, uint8_t addr, uint8_t reg, uint8_t val);
int i2c_read_reg(const struct i2c_adapter *adap, uint8_t addr, uint8_t reg);
int i2c_init(const struct i2c_adapter *adap);
