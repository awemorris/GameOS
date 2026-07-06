/*
 * Simple Console
 */

#include <sys/hal/cons.h>
#include <sys/kcrt/kcrt.h>	/* crt_memset16 */
#include "../i386/irq.h"	/* irq_enter_isr(), irq_leave_isr() */
#include "../i386/asm.h"	/* _asm_outb, SYS_START */

/* vram address */
#define VRAM_TEXT_ADDR		(0xA0000)
#define VRAM_ATTR_ADDR		(0xA2000)

/* vram character format */
#define MK_VRAMCHAR(c, attr)	((c) | ((attr) << 8))

/* screen setting */
static uint8 *vram_text = (uint8 *)VRAM_TEXT_ADDR + SYS_START;
static uint8 *vram_attr = (uint8 *)VRAM_ATTR_ADDR + SYS_START;
static int columns = 80;
static int lines = 25;

/* console status */
static int cur_col = 0;
static int cur_line = 0;
static uint8 cur_attr = 0xbf;

/* forward declaration */
static void clear_screen();
static void put_char(int c);
static void set_cursor_pos(int line, int col);
static void scroll_line();
static int get_keyboard_char();

/*
 * 簡易コンソールを初期化する
 */
void cons_init()
{
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
	crt_memset(vram_text, 0, 160 * 25);
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
		*(vram_text + cur_line * 160 + cur_col * 2) = c;
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
	//set_cursor_pos(cur_line, cur_col);
}

/* move cursor */
static void set_cursor_pos(int line, int col)
{
        uint16 addr = line * 80 + col;

        /* Wait for GDC FIFO READY. */
        while ((asm_inb(0x60) & 0x04) == 0)
		;

        /* Send CSRW (Cursor Write: 0x49) control command to GDC. */
        asm_outb(0x60, 0x49);

        /* Send lower 8-bit of the cursor address. */
        asm_outb(0x62, addr & 0xff);

        /* Send higher 8-bit of the cursor address. */
        asm_outb(0x62, (addr >> 8) & 0xff);

        /* Update the position. */
        cur_line = line;
        cur_col  = col;
}

/* scroll 1-line */
static void scroll_line()
{
	crt_memcpy(vram_text, vram_text + 160, 160 * 24);
	crt_memset(vram_text + 160 * 24, 0, 160);
}

/*
 * 簡易キーボードドライバ
 */

#define KBD_BUF_SIZE	(256)

int	kbd_buf[KBD_BUF_SIZE];
int	kbd_buf_len = 0;

/*
 * Input one character from keyboard.
 */
int get_keyboard_char()
{
        uint8 scancode;

        /* キーボードから1文字以上受信する */
        for(;;) {
                irq_enter_isr(IRQ_KEYBOARD);

                kbd_buf_len = 0;
                while(asm_inb(0x43) & 2) {
                        scancode = asm_inb(0x41);
                        if(kbd_buf_len < KBD_BUF_SIZE)
                                kbd_buf[kbd_buf_len++] = scancode;
                }

                irq_leave_isr(IRQ_KEYBOARD);

                if(kbd_buf_len == 0)
                        continue;

                /*
                 * Bit 7 is ON when the key is released.
                 * We use the press edge and ignore release edge.
                 */
                if((kbd_buf[0] & 0x80) != 0)
                        continue;
                        
                break;
        }

        /* TODO: scancode to ASCII. */
        return kbd_buf[0];
}
