#include "sys/kcrt/kcrt.h"
#include "sys/hal/irq.h"

/*
 * アサーションメッセージを表示して停止する
 */
void crt_assert(const char *file, int line, const char *exp)
{
	/* アサーション情報を出力する */
	crt_printf(	"\n"
				"---Assertion failed---\n"
				" file: %s\n"
				" line: %d\n"
				" expression: %s\n",
				file, line, exp);

	/* 割り込みを禁止する */
	irq_acquire_lock();

	/* 無限ループする */
	for(;;)
		;
}

/*
 * クラッシュする
 */
void crt_fatal(const char *file, int line, const char *s)
{
	/* FATALメッセージを出力する */
	crt_printf(	"\n"
				"[FATAL] %s\n"
				" file: %s\n"
				" line: %d\n"
				"\n"
				"... _|~|O", s, file, line);


	/* 割り込みを禁止する */
	irq_acquire_lock();

	/* 無限ループする */
	for(;;)
		;
}
