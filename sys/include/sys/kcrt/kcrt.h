#ifndef SYS_KCRT_KCRT_H
#define SYS_KCRT_KCRT_H

#include <sys/types.h>

#define assert(e)	((e) ? (void)0 : assert_impl(__FILE__, __LINE__, #e))
#define fatal(msg)	fatal_impl(__FILE__, __LINE__, msg)

/*
 * crtstring.c
 */
size_t strlen(const char *s);
void *memset(void *s, int c, size_t n);
void *memset16(uint16 *s, uint16 c, size_t n);
void *memset32(uint32 *s, uint32 c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

/*
 * crtmalloc.c
 */
void *malloc(size_t size);
void free(void *ptr);

/*
 * crtprintf.c
 */
int putchar(int c);
int puts(const char *s);
int printf(const char *format, ...);

/*
 * crtgets.c
 */
size_t gets_safe(char *buf, size_t buf_size);

/*
 * crtmisc.c
 */
void assert_impl(const char *file, int line, const char *exp);
void fatal_impl(const char *file, int line, const char *s);

#endif
