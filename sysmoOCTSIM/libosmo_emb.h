/* Tear-free 64-bit load/store for Cortex-M4.
 *
 * LDRD/STRD are restartable: if an interrupt arrives mid-instruction the
 * core restarts the access from scratch, so the result is always consistent.
 * This requires 8-byte alignment (unaligned LDRD silently becomes two
 * separate word accesses, losing the restart guarantee).
 *
 * Worst-case outcome is being off by one tick (1 ms) which is irrelevant
 * for the timeout use-case.
 *
 * To enforce the alignment requirement the underlying
 * uint64_t is wrapped in a struct with 8-byte
 * alignment via alignas(8) + _Generic to ensure types.
 */

#include <stdint.h>
#include <stdalign.h>

typedef struct {
	alignas(8) volatile uint64_t _v;
} tearfree_u64_t;

#if defined(__arm__)
static inline uint64_t _ldrd_u64(const tearfree_u64_t *p)
{
	uint32_t lo, hi;
	__asm__ volatile(
		"ldrd %0, %1, [%2]\n"
		: "=&r"(lo), "=&r"(hi)
		: "r"(&p->_v)
		: "memory");
	return ((uint64_t)hi << 32) | lo;
}

static inline void _strd_u64(tearfree_u64_t *p, uint64_t v)
{
	uint32_t lo = (uint32_t)v;
	uint32_t hi = (uint32_t)(v >> 32);
	__asm__ volatile(
		"strd %1, %2, [%0]\n"
		: : "r"(&p->_v), "r"(lo), "r"(hi)
		: "memory");
}
#else
/* host */
static inline uint64_t _ldrd_u64(const tearfree_u64_t *p)
{
	return p->_v;
}
static inline void _strd_u64(tearfree_u64_t *p, uint64_t v)
{
	p->_v = v;
}
#endif

/* _Generic ensures matched types or bust */
#define ldrd_u64(p) _Generic((p), \
	tearfree_u64_t *:       _ldrd_u64, \
	const tearfree_u64_t *: _ldrd_u64)(p)

#define strd_u64(p, v) _Generic((p), \
	tearfree_u64_t *: _strd_u64)((p), (v))

uint64_t get_jiffies(void);
void store_jiffies(uint64_t j);
