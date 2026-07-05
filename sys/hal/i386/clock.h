/*
 * clock.h
 *  - interval timer management
 */

#ifndef _SYS_ARCH_X86_CLOCK_H_
#define _SYS_ARCH_X86_CLOCK_H_

/*
 * clock.c
 */
void clock_init(void);
void clock_irq_handler(void);
void clock_handler(void);

#endif
