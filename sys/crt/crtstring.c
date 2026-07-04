#include "sys/crt/crt.h"

/*
 * NUL終端文字列の長さを取得する
 */
int crt_strlen(const char *s)
{
	int len = 0;
	while(*s++)
		len++;
	return len;
}

/*
 * メモリブロックに8ビット値をセットする
 */
void *crt_memset(void *s, int c, size_t n)
{
	uint8 *dst = (uint8 *) s;

	for(; n>0; n--)
		*dst++ = c;

	return s;
}

/*
 * メモリブロック16ビット値をセットする
 */
void *crt_memset16(uint16 *s, uint16 c, size_t n)
{
	uint16 *dst = (uint16 *) s;

	for(; n>0; n--)
		*dst++ = c;

	return s;
}

/*
 * メモリブロック32ビット値をセットする
 */
void *crt_memset32(uint32 *s, uint32 c, size_t n)
{
	uint32 *dst = s;

	for(; n>0; n--)
		*dst++ = c;

	return s;
}

/*
 * メモリブロックをコピーする
 */
void *crt_memcpy(void *dest, const void *src, size_t n)
{
	uint8		*d = (uint8 *) dest;
	const uint8	*s = (const uint8 *) src;

	for(; n>0; n--)
		*d++ = *s++;

	return dest;
}
