#pragma once
/* Host-side jiffies emulation
 * On the host every access is a plain memory access; the typed wrapper exists
 * only purely for type compatibility with code shared with the embedded target. */

#include <stdint.h>
#include <stdalign.h>

typedef struct {
	alignas(8) volatile uint64_t _v;
} tearfree_u64_t;

static inline uint64_t _ldrd_u64(const tearfree_u64_t *p) { return p->_v; }
static inline void     _strd_u64(tearfree_u64_t *p, uint64_t v) { p->_v = v; }

#define ldrd_u64(p) _Generic((p), \
	tearfree_u64_t *:       _ldrd_u64, \
	const tearfree_u64_t *: _ldrd_u64)(p)

#define strd_u64(p, v) _Generic((p), \
	tearfree_u64_t *: _strd_u64)((p), (v))

uint64_t get_jiffies(void);
