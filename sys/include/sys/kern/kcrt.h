/*
 * Kernel CRT
 */

#ifndef SYS_KERN_KCRT_H
#define SYS_KERN_KCRT_H

#include <sys/types.h>

#define assert(e)	((e) ? (void)0 : assert_impl(__FILE__, __LINE__, #e))
#define fatal(msg)	fatal_impl(__FILE__, __LINE__, msg)

size_t strlen(const char *s);
void *memset(void *s, int c, size_t n);
void *memset16(uint16_t *s, uint16_t c, size_t n);
void *memset32(uint32_t *s, uint32_t c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

void *malloc(size_t size);
void free(void *ptr);

int putchar(int c);
int puts(const char *s);
int printf(const char *format, ...);

size_t gets_safe(char *buf, size_t buf_size);

void assert_impl(const char *file, int line, const char *exp);
void fatal_impl(const char *file, int line, const char *s);

#endif
