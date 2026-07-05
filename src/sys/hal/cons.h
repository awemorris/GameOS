/*
 * cons.h
 *  - kernel debug console support
 */

#ifndef _KERNEL_ARCH_CONS_H_
#define _KERNEL_ARCH_CONS_H_

#include <sys/types.h>


/*
 * cons.c
 */

void  cons_cls(void);
void  cons_putc(int c);
void  cons_puts(const char *s);
int   cons_getc(void);


#endif
