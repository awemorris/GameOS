/*
 * アドレス空間管理部
 */

#include "sys/arch/x86/univ.h"
#include "sys/arch/x86/asm.h"
#include "sys/crt/crt.h"

struct univ_info *univ_list_head;	/* univリストの先頭*/
univ_t	*cur_univ;					/* 現在選択されているuniv */
int		free_id_top;				/* 未使用IDの先頭 */

/*
 * アドレス空間管理部を初期化する
 */
void univ_init()
{
	cur_univ = UNIV_SYS;
	free_id_top = 1;
}

/*
 * アドレス空間を作成する
 */
univ_t univ_create()
{
	struct univ_info *ui;
	int i;

	/* 構造体のメモリを確保して初期化する */
	ui = (struct univ_info *) crt_malloc(sizeof(struct univ_info));
	ui->univ_id		= free_id_top++;
	ui->ptbl_head	= NULL;
	ui->next		= NULL;

	/* PDTを初期化する */
	for(i=0; i<1024; i++)
		ui->pdt[i] = 0;
	for(i=0; i<128; i++)
		ui->pdt[512+i] = (i*0x400000)|(PTE_PRESENT|PTE_USER|PTE_BIG|PTE_WRITE);

	/* univ_tにキャストして返す */
	return (univ_t) ui;
}

/*
 * アドレス空間を切り替える
 */
void univ_switch(univ_t u)
{
	struct univ_info *ui;

	/* 変更がない場合 */
	if(u == cur_univ)
		return;

	/* univ_infoにキャストする */
	ui = (struct univ_info *)u;

	/* PDTを切り替える */
	asm_load_cr3((uint32)ui->pdt - SYS_START);

	crt_puts("\n[univ changed!]");
}

/*
 * univ値が正しいかチェックする
 */
int univ_is_valid(univ_t u)
{
	struct univ_info *ui;

	ui = (struct univ_info *)u;

	/* カーネル空間の場合 */
	if(ui == UNIV_SYS)
		return 1;	/* 正しい値 */

	/* 不正な値*/
	return 0;
}
