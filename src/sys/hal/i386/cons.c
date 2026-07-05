/*
 * 簡易コンソール
 */

#include "sys/hal/i386/cons.h"
#include "sys/hal/i386/irq.h"	/* irq_enter_isr(), irq_leave_isr() */
#include "sys/hal/i386/asm.h"	/* _asm_outb, SYS_START */
#include "sys/kcrt/kcrt.h"		/* crt_memset16 */


/* vram address */
#define VRAM_ADDR				(0xb8000)

/* vram character format */
#define MK_VRAMCHAR(c,attr)		((c) | ((attr) << 8))

/* screen setting */
typedef uint16 VRAMCHAR;
VRAMCHAR 	*vram	= (VRAMCHAR *)(VRAM_ADDR + SYS_START);
static int	columns	= 80;
static int	lines	= 25;

/* console status */
static int		cur_col		= 0;
static int		cur_line	= 0;
static uint8	cur_attr	= 0x07;

/* forward declaration */
static void clear_screen();
static void put_char(int c);
static void set_cursor_pos(int line, int col);
static void scroll_line();
static int	get_keyboard_char();

/*
 * 簡易コンソールを初期化する
 */
void cons_init()
{
	/* TODO: 画面サイズ取得など */

	/* 画面をクリアする */
	clear_screen();
}

/*
 * clear screen
 */
void cons_cls()
{
	/* 画面をクリアする */
	clear_screen();
}

/*
 * 文字を出力する
 */
void cons_putc(int c)
{
	/* 文字を出力する */
	put_char(c);
}

/*
 * 文字列を出力する
 */
void cons_puts(const char *s)
{
	/* 一文字ずつ出力する */
	while(*s != '\0')
		put_char(*s++);
}

/*
 * 1文字入力する
 */
int cons_getc()
{
	return get_keyboard_char();
}


/*
 * clear screen
 */
static void clear_screen()
{
	VRAMCHAR space_char;

	space_char = MK_VRAMCHAR(' ', cur_attr);
	crt_memset16(vram, space_char, columns * lines);
	set_cursor_pos(0, 0);
}

/*
 * put a character
 */
static void put_char(int c)
{
	/* 文字ごとに処理を行う */
	switch(c) {
	case '\n':
		/* 改行する */
		cur_line++, cur_col = 0;
		break;
	default:
		/* 文字と属性を書き込む */
		*(vram + cur_col + cur_line*columns) = MK_VRAMCHAR(c, cur_attr);
		cur_col++;
		break;
	}

	/* カーソル位置が行の右端を越えた場合、次の行に移動する */
	if(cur_col == columns)
		cur_col = 0, cur_line++;

	/* カーソル位置が最下行を越えた場合、スクロールする */
	if(cur_line == lines) {
		scroll_line();
		cur_line = lines - 1, cur_col = 0;
	}

	/* カーソル位置を更新する */
	set_cursor_pos(cur_line, cur_col);
}

/*
 * move cursor
 */
static void set_cursor_pos(int line, int col)
{
	uint32 addr;

	addr = col + line * columns;

	asm_outb(0x3d4, 0x0e);
	asm_outb(0x3d5, addr >> 8);
	asm_outb(0x3d4, 0x0f);
	asm_outb(0x3d5, addr & 0xff);

	cur_line = line;
	cur_col  = col;
}

/*
 * scroll 1-line
 */
static void scroll_line()
{
	VRAMCHAR	*p, space_char;
	uint32		count, blank, i;

	/* 先頭行から順に1行下の文字をセットしていく */
	p = vram;
	count = columns * (lines-1);
	for(i=0; i<count; i++, p++)
		*p = *(p + columns);

	/* 最下行をクリアする */
	space_char = ' ' | (cur_attr << 8);
	crt_memset16(p, space_char, columns);
}


/*
 * 簡易キーボードドライバ
 */

#define KBD_BUF_SIZE	(256)

int	kbd_buf[KBD_BUF_SIZE];
int	kbd_buf_len = 0;


/*
 * キーボードから1文字入力する
 */
int get_keyboard_char()
{
	uint8  scancode;

	/* キーボードから1文字以上受信する */
	for(;;) {
		/* 割り込みを待って割り込み処理を開始する */
		irq_enter_isr(IRQ_KEYBOARD);

		/* キーボードコントローラと通信する */
		kbd_buf_len = 0;
		while(asm_inb(0x64) & 1) {
			scancode = asm_inb(0x60);
			if(kbd_buf_len < KBD_BUF_SIZE)
				kbd_buf[kbd_buf_len++] = scancode;
		}

		/* 割り込み処理を完了する */
		irq_leave_isr(IRQ_KEYBOARD);

		/* 受信バッファをチェックする */
		if(kbd_buf_len == 0)
			continue;
		if(scancode == 0xE0 || scancode == 0xE1)
			continue;
		if((kbd_buf[0] & 0x80) != 0)
			continue;
		break;
	}

	/* キーコード */
	return kbd_buf[0];
}
