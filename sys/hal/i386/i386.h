/*
 * Kernel HAL for i386
 */

#ifndef SYS_HAL_I386_ASM_H
#define SYS_HAL_I386_ASM_H

#include <sys/types.h>
#include "defs.h"

/*
 * i386 Assembly Routines
 */
void asm_cli(void);
void asm_sti(void);
void asm_outb(uint16_t port, uint8_t data);
void asm_outw(uint16_t port, uint16_t data);
uint8_t asm_inb(uint16_t port);
uint16_t asm_inw(uint16_t port);
void asm_fxsave(void *save_area);
void asm_fxrstor(void *save_area);
void asm_fnsave(void *save_area);
void asm_frstor(void *save_area);
uint32_t asm_get_eflags(void);
uint32_t asm_get_eip(void);
uint32_t asm_get_esp(void);
uint32_t asm_get_cr3(void);
void asm_hlt(void);
void asm_lidt(void *idt_desc);
void asm_load_cr3(uint32_t addr);
void asm_flush_tlb(void);

/*
 * i386 Platform BSP IRQ Interface
 */
void bsp_irq_init(void);
void bsp_irq_set_affinity(int irq, struct hal_cpu_mask cpu_mask);
bool bsp_irq_disable(void);
void bsp_irq_enable(void);
void bsp_irq_mask(int irq_num);
void bsp_irq_unmask(int irq_num);
int bsp_irq_get_in_service(void);
void bsp_irq_send_eoi(int irq);
void bsp_irq_set_handler(int irq_num, void (*func)(void *p), void *arg);
void bsp_irq_handler(int irq_num);

/*
 * i386 Platform BSP Timer Interface
 */
void bsp_pit_init(void);
void bsp_pit_set_freq(uint32_t freq);
uint64_t bsp_pit_get_tick(void);

/*
 * i386 Platform BSP Console Interface
 */
void bsp_cons_init(void);
void bsp_cons_putc(int c);
int bsp_cons_getc(void);
void bsp_cons_clear(void);
void bsp_cons_move_cursor(int line, int col);

#endif
