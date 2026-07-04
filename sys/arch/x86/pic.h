/*
 * pic.h
 *  - PIC(programmable interrupt controller) management
 */

#ifndef _SYS_ARCH_X86_PIC_H_
#define _SYS_ARCH_X86_PIC_H_

#include <gravity.h>

/*
 * pic.c
 */
void pic_init(void);
void pic_set_irq_mask(int irq_num, int mask);
int  pic_get_irq_in_service(void);
void pic_send_eoi(int irq_num);


#endif
