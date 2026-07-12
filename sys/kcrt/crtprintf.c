#include <sys/kcrt/kcrt.h>
#include <sys/hal/cons.h>

static void put_dec(int n, int column, int sign);
static void put_hex(int n, int column, int capital);

int putchar(int c)
{
	cons_putc(c);

	return 1;
}

int puts(const char *s)
{
	while(*s != '\0')
		cons_putc(*s++);

	cons_putc('\n');

	return 1;
}

int printf(const char *format, ...)
{
	const int OPT_LEN_MAX = 4;	/* Maximum option length. */

	void **argp;
	int escape;
	char opt[OPT_LEN_MAX];		/* option ("04" of "%04d") */
	int opt_len;

	argp	= (void *) &format;
	escape	= 0;
	opt_len	= 0;

	while(*format != '\0') {
		int output = -1;
		char c = *format++;
		int opt_flag = 0;

		if(!escape) {
			if(c == '%') escape = 1;
			else		 cons_putc(c);
			continue;
		}

		/* When escaped: */
		switch(c) {
		case 's':
			cons_puts((const char *) *(++argp));
			break;
		case 'd':
			put_dec((int) *(++argp),
				(opt_len > 0 && opt[0] == '0') ? (opt[1] - '0') : 0,
				1 /* signed */);
			break;
		case 'u':
			put_dec((int) *(++argp),
				(opt_len > 0 && opt[0] == '0') ? (opt[1] - '0') : 0,
				0 /* unsigned */);
			break;
		case 'x':
			put_hex((int) *(++argp),
				(opt_len > 0 && opt[0] == '0') ? (opt[1] - '0') : 0,
				0 /* small */);
			break;
		case 'X':
			put_hex((int) *(++argp),
				(opt_len > 0 && opt[0] == '0') ? (opt[1] - '0') : 0,
				1 /* CAPITAL */);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			opt_flag = 1;
			if(opt_len < OPT_LEN_MAX)
				opt[opt_len++] = c;
			break;
		default:
			putchar(c);
			break;
		}
		if(!opt_flag) {
			escape = 0;
			opt_len	= 0;
		}
	}

	/* XXX: Returns 0. */
	return 0;
}

static void
put_dec(
	int n,
	int column,
	int sign)
{
	char bcd[20];
	int shown;
	int i;

	if (sign && n < 0) {
		n = 0 - n;
		putchar('-');
	}

	/* Calc from lower. */
	for (i = 0; i < 20; i++) {
		bcd[i] = ((unsigned) n) % 10;
		n = ((unsigned) n) / 10;
	}

	/* Print from higher. */
	shown = 0;
	for(i=19; i>=0; i--) {
		/* Don't put heading zeros.*/
		if(bcd[i] == 0 &&
		   i != 0 &&
		   shown == 0 &&
		   column <= i)
			continue;

		cons_putc('0' + bcd[i]);

		shown = 1;
	}
}

static void
put_hex(
	int n,
	int column,
	int capital)
{
	char hex[16];
	int shown;
	int letter;	/* 'a' or 'A' */
	int i;

	/* Calc from lower. */
	for(i=0; i<16; i++) {
		hex[i] = n & 0x0f;
		n = ((unsigned) n) >> 4;
	}

	/* Print from higher. */
	shown	= 0;
	letter	= capital ? 'A' : 'a';
	for (i = 15; i >= 0; i--) {
		if(hex[i] == 0 &&
		   i != 0 &&
		   shown == 0 &&
		   column <= i)
			continue;

		cons_putc((hex[i] < 10) ? ('0' + hex[i]) : (letter + hex[i] - 10));

		shown = 1;
	}
}
