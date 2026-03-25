/* Tear-free 64-bit load/store for Cortex-M4.
 *
 * LDRD/STRD are restartable: if an interrupt arrives mid-instruction the
 * core restarts the access from scratch, so the result is always consistent.
 * This requires 8-byte alignment (unaligned LDRD silently becomes two
 * separate word accesses, losing the restart guarantee).
 *
 * Worst-case outcome is being off by one tick (1 ms) which is irrelevant
 * for the timeout use-case.
 */

#include <stdint.h>

#if defined(__arm__)
static inline uint64_t ldrd_u64(const volatile uint64_t *p)
{
	uint32_t lo, hi;
	__asm__ volatile(
		"ldrd %0, %1, [%2]\n"
		: "=&r"(lo), "=&r"(hi)
		: "r"(p)
		: "memory");
	return ((uint64_t)hi << 32) | lo;
}

static inline void strd_u64(volatile uint64_t *p, uint64_t v)
{
	uint32_t lo = (uint32_t)v;
	uint32_t hi = (uint32_t)(v >> 32);
	__asm__ volatile(
		"strd %1, %2, [%0]\n"
		: : "r"(p), "r"(lo), "r"(hi)
		: "memory");
}
#else
/* host */
static inline uint64_t ldrd_u64(const volatile uint64_t *p)
{
	return *p;
}
static inline void strd_u64(volatile uint64_t *p, uint64_t v)
{
	*p = v;
}
#endif

uint64_t get_jiffies(void);
void store_jiffies(uint64_t j);
