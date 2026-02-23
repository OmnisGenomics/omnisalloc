#ifndef JEMALLOC_INTERNAL_SPIN_H
#define JEMALLOC_INTERNAL_SPIN_H

#include "jemalloc/internal/jemalloc_preamble.h"

#if defined(__linux__) && (defined(__aarch64__) || defined(__arm64__)) && \
    (defined(__GNUC__) || defined(__clang__))
#  include <sys/auxv.h>
#endif

#define SPIN_INITIALIZER {0U}

typedef struct {
	unsigned iteration;
} spin_t;

static inline void
spin_cpu_spinwait(void) {
#if defined(__linux__) && (defined(__aarch64__) || defined(__arm64__)) && \
    (defined(__GNUC__) || defined(__clang__))
	/*
	 * Use SB only when runtime CPU capabilities report support.  Assembler
	 * support alone is insufficient, since binaries may run on older cores.
	 */
	static int arm_has_sb_instruction = -1;
	int has_sb = __atomic_load_n(&arm_has_sb_instruction, __ATOMIC_RELAXED);
	if (has_sb == -1) {
#  ifdef HWCAP_SB
		has_sb = (getauxval(AT_HWCAP) & HWCAP_SB) != 0 ? 1 : 0;
#  else
		has_sb = 0;
#  endif
		__atomic_store_n(&arm_has_sb_instruction, has_sb,
		    __ATOMIC_RELAXED);
	}
	if (has_sb) {
		/* SB instruction encoding (arm64). */
		__asm__ volatile(".inst 0xd50330ff");
		return;
	}
#endif
#  if HAVE_CPU_SPINWAIT
	CPU_SPINWAIT;
#  else
	volatile int x = 0;
	x = x;
#  endif
}

static inline void
spin_adaptive(spin_t *spin) {
	volatile uint32_t i;

	if (spin->iteration < 5) {
		for (i = 0; i < (1U << spin->iteration); i++) {
			spin_cpu_spinwait();
		}
		spin->iteration++;
	} else {
#ifdef _WIN32
		SwitchToThread();
#else
		sched_yield();
#endif
	}
}

#undef SPIN_INLINE

#endif /* JEMALLOC_INTERNAL_SPIN_H */
