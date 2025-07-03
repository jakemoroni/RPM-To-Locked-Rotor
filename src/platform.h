/* Copyright (C) 2024 Jacob Moroni (opensource@jakemoroni.com)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Implements simple platform-specific helper functions.
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

/* I think F_CPU is used by some AVR header files, so define
 * it here first.
 */
#define F_CPU                          8000000u

#include <stdint.h>
#include <avr/io.h>

/* Similar semantics to the Linux kernel spinlock.
 * Since this is a simple uC, we just need to disable
 * interrupts.
 */
static inline uint8_t spin_lock_irqsave(void)
{
	uint8_t flags;
	asm volatile("in %0, %1\n\t"
		     "cli\n\t"
		     : "=r" (flags)
		     : "I" (_SFR_IO_ADDR(SREG))
		     : "memory");
	__sync_synchronize(); /* Not strictly necessary due to memory clobber above. */
	return flags;
}

static inline void spin_unlock_irqrestore(uint8_t flags)
{
	__sync_synchronize();
	asm volatile("out %0, %1" :: "I" (_SFR_IO_ADDR(SREG)), "r" (flags) : "memory");
}

#endif /* PLATFORM_H_ */
