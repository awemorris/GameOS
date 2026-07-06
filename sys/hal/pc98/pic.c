#include "../i386/pic.h"
#include "../i386/int.h"
#include "../i386/asm.h"

#define PIC_MASTER_PORT1	0x0000
#define PIC_MASTER_PORT2	0x0002
#define PIC_SLAVE_PORT1		0x0008
#define PIC_SLAVE_PORT2		0x000a

#define SLAVE_IRQ		7

/*
 * Initialize the PIC.
 */
void pic_init()
{
	/* Initialize the 8259A master. */
	asm_outb(PIC_MASTER_PORT1, 0x11);		/* Start init, edge-triggered / cascaded */
	asm_outb(PIC_MASTER_PORT2, INT_IRQ_BASE);	/* INT E0h-EFh */
	asm_outb(PIC_MASTER_PORT2, 1 << SLAVE_IRQ);	/* Connect to the slave */
	asm_outb(PIC_MASTER_PORT2, 0x01);		/* 80x86 mode */

	/* Initialize the 8259A slave. */
	asm_outb(PIC_SLAVE_PORT1, 0x11);		/* Start init, edge-triggered / cascaded */
	asm_outb(PIC_SLAVE_PORT2, INT_IRQ_BASE + 8);	/* INT E8h-EFh */
	asm_outb(PIC_SLAVE_PORT2, 1 << SLAVE_IRQ);	/* Connect to the master */
	asm_outb(PIC_SLAVE_PORT2, 0x01);		/* 80x86 mode */

	/* Mask all IRQs. */
	asm_outb(PIC_MASTER_PORT2, 0xff);
	asm_outb(PIC_SLAVE_PORT2, 0xff);
}

/*
 * Set the IRQ mask.
 */
void pic_set_irq_mask(
	int	irq_num,	/* IRQ number */
	int	mask)		/* 0: allow, 1: disallow */
{
	if(irq_num < 8) {
		uint8 imr = asm_inb(PIC_MASTER_PORT2);
		if(mask) imr |=  (1 << irq_num);
		else     imr &= ~(1 << irq_num);
		asm_outb(PIC_MASTER_PORT2, imr);
	} else {
		uint8 imr = asm_inb(PIC_SLAVE_PORT2);
		if(mask) imr |=  (1 << (irq_num&7));
		else     imr &= ~(1 << (irq_num&7));
		asm_outb(PIC_SLAVE_PORT2, imr);
	}
}

/*
 * Get the in-service IRQ number.
 */
int pic_get_irq_in_service(void)
{
	uint8 in_service;
	int irq_num;

	/* Read ISR register to know in-service IRQ number. */
	asm_outb(PIC_MASTER_PORT1, 0x0B);
	in_service = asm_inb(PIC_MASTER_PORT1);

	/* Search the bit to get the IRQ number. */
	irq_num = -1;
	while(in_service != 0) {
		irq_num++;
		in_service >>= 1;
	}

	/* If slave IRQ. */
	if (irq_num == 7) {
		asm_outb(PIC_SLAVE_PORT1, 0x0B);
		in_service = asm_inb(PIC_SLAVE_PORT1);

		irq_num = 7;
		while(in_service != 0) {
			irq_num++;
			in_service >>= 1;
		}
	}

	return irq_num;
}

/*
 * Send EOI.
 */
void pic_send_eoi(int irq_num)
{
	/* Sent EOI. */
	if(irq_num <= 7) {
		/* Send EOI to master. */
		asm_outb(PIC_MASTER_PORT1, 0x20);
	} else {
		/* Send EOI to slave. */
		asm_outb(PIC_SLAVE_PORT1, 0x20);

		/* Read slave ISR. */
		asm_outb(PIC_SLAVE_PORT1, 0x0B);

		/* If there is no remaining ISR: */
		if(asm_inb(PIC_SLAVE_PORT1) == 0) {
			/* Also send EOI to master. */
			asm_outb(PIC_MASTER_PORT1, 0x20);
		}
	}
}
