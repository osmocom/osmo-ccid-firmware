#pragma once
#include <stdbool.h>

enum ncn8025_sim_voltage {
	SIM_VOLT_3V0 = 0,
	SIM_VOLT_5V0 = 2,
	SIM_VOLT_1V8 = 3
};

enum ncn8025_sim_clkdiv {
	SIM_CLKDIV_1 = 1,
	SIM_CLKDIV_2 = 3,
	SIM_CLKDIV_4 = 2,
	SIM_CLKDIV_8 = 0,
};

extern const unsigned int ncn8025_div_val[];

struct ncn8025_settings {
	bool rstin;	/* Reset signal (true: asserted low) */
	bool cmdvcc;	/* Command VCC pin. Activation sequence Enable (true: active low) */
	bool simpres;	/* high: active */
	bool led;	/* high: active */
	bool interrupt;	/* high: active */
	enum ncn8025_sim_clkdiv clkdiv;	/* raw 2bit value */
	enum ncn8025_sim_voltage vsel;	/* raw 2bit value */
};

int ncn8025_set(uint8_t slot, const struct ncn8025_settings *set);
int ncn8025_get(uint8_t slot, struct ncn8025_settings *set);
bool ncn8025_interrupt_level(uint8_t slot);
int ncn8025_init(unsigned int slot);
void ncn8025_dump(const struct ncn8025_settings *set);
