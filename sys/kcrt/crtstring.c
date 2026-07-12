#include <sys/kcrt/kcrt.h>

size_t
strlen(
	const char *s)
{
	size_t len;

	len = 0;
	while(*s++)
		len++;

	return len;
}

void *
memset(
	void *s,
	int c,
	size_t n)
{
	uint8 *dst = (uint8 *)s;

	for(; n>0; n--)
		*dst++ = c;

	return s;
}

void *
memset16(
	uint16 *s,
	uint16 c,
	size_t n)
{
	uint16 *dst = (uint16 *)s;

	for(; n>0; n--)
		*dst++ = c;

	return s;
}

void *
memset32(
	uint32 *s,
	uint32 c,
	size_t n)
{
	uint32 *dst = s;

	for(; n>0; n--)
		*dst++ = c;

	return s;
}

void *
memcpy(
	void *dest,
	const void *src,
	size_t n)
{
	uint8 *d;
	const uint8 *s;

	d = (uint8 *)dest;
	s = (const uint8 *) src;

	for(; n>0; n--)
		*d++ = *s++;

	return dest;
}
