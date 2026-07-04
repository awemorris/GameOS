#ifndef _SYS_CRT_CRT_H_
#define _SYS_CRT_CRT_H_

#include <gravity.h>

#define CRT_ASSERT(e)	((e)?(void)0:crt_assert(__FILE__,__LINE__,#e))
#define CRT_FATAL(msg)	crt_fatal(__FILE__,__LINE__,msg)

/*
 * crtstring.c
 */
int crt_strlen(const char *s);
void *crt_memset(void *s, int c, size_t n);
void *crt_memset16(uint16 *s, uint16 c, size_t n);
void *crt_memset32(uint32 *s, uint32 c, size_t n);
void *crt_memcpy(void *dest, const void *src, size_t n);

/*
 * crtmalloc.c
 */
void *crt_malloc(size_t size);
void crt_free(void *ptr);

/*
 * crtprintf.c
 */
int crt_putchar(int c);
int crt_puts(const char *s);
void crt_puthex8(uint8 n);
void crt_puthex16(uint16 n);
void crt_puthex32(uint32 n);
int crt_printf(const char *format, ...);
void crt_putdec(int n, int column, int sign);
void crt_puthex(int n, int column, int capital);

/*
 * crtmisc.c
 */
void crt_assert(const char *file, int line, const char *exp);
void crt_fatal(const char *file, int line, const char *s);

#endif
