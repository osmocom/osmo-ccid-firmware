#pragma once
/* Host-side jiffies emulation */

#include <stdint.h>

static inline uint64_t ldrd_u64(const volatile uint64_t *p)
{
	return *p;
}
static inline void strd_u64(volatile uint64_t *p, uint64_t v)
{
	*p = v;
}

uint64_t get_jiffies(void);
