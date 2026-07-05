#include "sys/kcrt/kcrt.h"
#include "sys/hal/cons.h"

/* 前方参照 */
static void put_dec(int n, int column, int sign);
static void put_hex(int n, int column, int capital);

/*
 * コンソールに1文字出力する
 */
int crt_putchar(int c)
{
	cons_putc(c);

	return 1;
}

/*
 * コンソールにNUL終端文字列を出力する
 */
int crt_puts(const char *s)
{
	while(*s != '\0')
		cons_putc(*s++);

	/* 改行する */
	cons_putc('\n');

	return 1;
}

/*
 * コンソールにフォーマット付き文字列を出力する
 */
int crt_printf(const char *format, ...)
{
	const int OPT_LEN_MAX = 4;		/* 最大オプション文字数 */

	void	**argp;				/* 可変パラメタポインタ */
	int		escape;				/* '%'エスケープ状態フラグ */
	char	opt[OPT_LEN_MAX];	/* オプション保持配列 ("%04d"の"04"部分) */
	int		opt_len;			/* オプション文字数 */

	argp	= (void *) &format;
	escape	= 0;
	opt_len	= 0;

	while(*format != '\0') {
		int		output = -1;
		char 	c = *format++;
		int		opt_flag = 0;

		/* 非エスケープ状態のとき */
		if(!escape) {
			if(c == '%') escape = 1;
			else		 cons_putc(c);
			continue;
		}

		/* エスケープ状態のとき */
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
			crt_putchar(c);
			break;
		}
		if(!opt_flag) {
			escape	= 0;	/* エスケープ状態を解除する */
			opt_len	= 0;	/* オプションをリセットする */
		}
	}

	return 0;	/* 出力文字数は返さない */
}

/*
 * コンソールに整数を10進表記で出力する
 */
static void put_dec(
	int	n,			/* 整数 */
	int	column,		/* 桁数(0なら先頭のゼロ値桁を表示しない) */
	int sign)		/* 符号付きフラグ */
{
	char	bcd[20];	/* 64bit, 20桁まで対応 */
	int		shown;		/* 先頭桁出力フラグ */
	int		i;

	/* 負数の場合 */
	if(sign && n < 0) {
		n = 0 - n;			/* 符号を反転する */
		crt_putchar('-');	/* マイナス符号を出力する */
	}

	/* 下位の桁から計算する */
	for(i=0; i<20; i++) {
		bcd[i] = ((unsigned) n) % 10;
		n = ((unsigned) n) / 10;
	}

	/* 上位の桁から表示する */
	shown = 0;
	for(i=19; i>=0; i--) {
		/* 先頭のゼロ値桁を表示しない */
		if(	bcd[i] == 0 &&	/* 桁がゼロである */
			i != 0		&&	/* 最下位桁でない */
			shown == 0	&&	/* ゼロでない桁が未出現 */
			column <= i)	/* 指定桁数より左側の桁である */
		{
			continue;
		}

		/* 桁を出力する */
		cons_putc('0' + bcd[i]);

		/* 先頭桁出力済みフラグをセットする */
		shown = 1;
	}
}

/*
 * コンソールに整数を16進表記で出力する
 */
static void put_hex(
	int	n,			/* 整数 */
	int	column,		/* 桁数(0なら先頭のゼロ値桁を表示しない) */
	int capital)	/* 大文字使用フラグ */
{
	char	hex[16];	/* 64bit, 16桁まで対応 */
	int		shown;		/* 先頭桁出力フラグ */
	int		letter;		/* 'a' or 'A' */
	int		i;

	/* 下位の桁から計算する */
	for(i=0; i<16; i++) {
		hex[i] = n & 0x0f;
		n = ((unsigned) n) >> 4;
	}

	/* 上位の桁から表示する */
	shown	= 0;
	letter	= capital ? 'A' : 'a';
	for(i=15; i>=0; i--) {
		/* 先頭のゼロ値桁を表示しない */
		if(	hex[i] == 0 &&	/* 桁がゼロである */
			i != 0		&&	/* 最下位桁でない */
			shown == 0	&&	/* ゼロでない桁が未出現 */
			column <= i)	/* 指定桁数より左側の桁である */
		{
			continue;
		}

		/* 桁を出力する */
		cons_putc((hex[i] < 10) ? ('0' + hex[i]) : (letter + hex[i] - 10));

		/* 先頭桁出力済みフラグをセットする */
		shown = 1;
	}
}
