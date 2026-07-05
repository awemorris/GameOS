/*
 * io.h
 *  - io access management
 */

#ifndef _SYS_ARCH_IO_H_
#define _SYS_ARCH_IO_H_

#include <gravity.h>


/*
 * io.c
 */
void   io_write8(int addr, uint8 value);
void   io_write16(int addr, uint8 value);
uint8  io_read8(int addr);
uint16 io_read16(int addr);


#endif
