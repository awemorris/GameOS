/*
 * sched.c
 *	- priority based round robin scheduler
 */

#include <sys/types.h>
#include <sys/kern/sched.h>
#include <sys/kcrt/kcrt.h>
#include <sys/hal/clock.h>
#include <sys/hal/irq.h>

#define CPU_MAX				(0)
#define GET_CPU_ID()		(0)

/* CPUごとのスケジュールリスト */
struct cpu_sched_list sched_list[CPU_MAX+1];

/* forward declaration */
static struct schedulable *do_schedule();
static void do_timeout();
static void activelist_add(struct schedulable **head, struct schedulable *t);
static void activelist_del(struct schedulable **head, struct schedulable *t);
static void activelist_turn(struct schedulable **head);
static void timewaitlist_add(struct schedulable **head, struct schedulable *t);
static void timewaitlist_del(struct schedulable **head, struct schedulable *t);

void int_set_resched_flag(void);	/* 再スケジュールフラグをセットする */

/*
 * スケジュール管理部を初期化する
 */
void sched_init()
{
	int cpu_id, i;
	struct cpu_sched_list *slist;

	/* CPUのスケジュールリストを取得する */
	cpu_id	= GET_CPU_ID();
	slist	= &sched_list[cpu_id];

	/* timewaitリストを初期化する*/
	slist->timewait_head = NULL;

	/* activeリストを初期化する */
	for(i=0; i<SCHED_PRIOR_LEVELS; i++)
		slist->active_head[i] = NULL;

	/* 現在実行中のタスク(初期化タスク)をアイドルタスクに設定する
	 * (初期化完了後はアイドルループを実行し、アイドルタスクとして再利用する) */
	slist->idle_task = task_get_current();
}

/*
 * タスクの所属スケジュールリストを変更する
 */
void sched_link(
	task_t	t,			/* タスク */
	int		list,		/* スケジュールリスト */
	int		priority,	/* プライオリティ[0 - (SCHED_PRIORITY_LEVELS-1)] */
	int		opt)		/* オプション(timewait: 待機時間マイクロ秒) */
{
	struct cpu_sched_list	*slist;
	struct schedulable		*s;
	irqlock_t	irqlock;

	CRT_ASSERT(list == SCHED_LIST_UNLINKED || list == SCHED_LIST_ACTIVE || list == SCHED_LIST_TIMEWAIT);
	CRT_ASSERT(priority >= 0 && priority < SCHED_PRIOR_LEVELS);

	/* schedulableにキャストする */
	s = (struct schedulable *)t;

	/* CPUのスケジュールリストを取得する */
	slist  = &sched_list[GET_CPU_ID()];

	/* 割り込み禁止区間 */
	ENTER_IRQLOCK(irqlock)
	{
		/* タスクがリストにリンクされていれば、そのリストから切り離す */
		switch(s->status) {
		case SCHED_LIST_UNLINKED:
			/* リンクされていない */
			break;
		case SCHED_LIST_ACTIVE:
			/* activeリストから切り離す */
			activelist_del(&slist->active_head[s->priority], s);
			break;
		case SCHED_LIST_TIMEWAIT:
			/* timewaitリストから切り離す */
			timewaitlist_del(&slist->timewait_head, s);
			break;
		default:
			CRT_FATAL("unexpected value");
			break;
		}
		s->status = SCHED_LIST_UNLINKED;

		/* 引数で指定されたリストにリンクする */
		switch(list) {
		case SCHED_LIST_UNLINKED:
			/* リンクしない */
			break;
		case SCHED_LIST_ACTIVE:
			/* activeリストにリンクする */
			s->status	= SCHED_LIST_ACTIVE;
			s->priority = priority;
			activelist_add(&slist->active_head[priority], s);
			break;
		case SCHED_LIST_TIMEWAIT:
			/* timewaitリストにリンクする */
			s->status	= SCHED_LIST_TIMEWAIT;
			s->priority = priority;
			s->timeout	= clock_get_tick_count() + opt;
			timewaitlist_add(&slist->timewait_head, s);
			break;
		default:
			CRT_FATAL("unexpected value");
			break;
		}
	}
	LEAVE_IRQLOCK(irqlock);
}

/*
 * ローカルクロック割り込みのハンドラ
 */
void sched_clock_handler(void)
{

	/* 再スケジュールを要求する */
	int_set_resched_flag();
}

/*
 * タスク切り替えを行う
 */
void sched_yield()
{
	struct schedulable *switch_to;

	/* 次に実行すべきタスクを探す */
	switch_to = do_schedule();

	/* タスクを切り替える */
	task_switch((task_t)switch_to);

	/* 切り替え先のタスクはここに戻ってくる。
	 * ただし、切り替え先が新規タスクの場合はここには戻らず、
	 * エントリポイントへジャンプする。 */
}

/*
 * スケジューリングを行い次に実行するタスクを求める
 */
static struct schedulable *do_schedule()
{
	int	i;
	struct cpu_sched_list	*slist;
	struct schedulable		*switch_to;

	/* CPUのスケジュールリストを取得する */
	slist = &sched_list[GET_CPU_ID()];

	/* timewaitリストのタイムアウトを処理する */
	if(slist->timewait_head != NULL)
		do_timeout();

	/* CPUのスケジューリングリストから優先度の最も高いタスクを選択する */
	for(i=0; i<SCHED_PRIOR_LEVELS; i++) {
		/* activeリストにタスクが存在する場合 */
		if(slist->active_head[i] != NULL) {
			switch_to = slist->active_head[i];		/* タスクを選択する */
			activelist_turn(&slist->active_head[i]);	/* リストを回転させる */
			return switch_to;	/* 選択したタスクを返す */
		}
	}

	/* 実行できるタスクがないのでアイドルタスクを返す */
	return slist->idle_task;
}

/* timewaitリストのタイムアウトを処理する */
static void do_timeout()
{
	clock_t	cpu_clock;
	struct schedulable		*s;
	struct cpu_sched_list	*slist;

	slist = &sched_list[GET_CPU_ID()];

	/* CPUのローカル時刻を取得する */
	cpu_clock = clock_get_tick_count();

	/* タイムアウトしているタスクをactiveリストに移す */
	s = slist->timewait_head;
	while(s->timeout < cpu_clock) {
		struct schedulable *save_next = s->next;

		/* timewaitリストから削除する */
		timewaitlist_del(&slist->timewait_head, s);

		/* activeリストに移動する */
		s->status = SCHED_LIST_ACTIVE;
		activelist_add(&slist->active_head[s->priority], s);

		/* sがactiveリストの先頭になるまでturnする */
		while(slist->active_head[s->priority] != s)
			activelist_turn(&slist->active_head[s->priority]);

		if(save_next == NULL)
			break;
		s = save_next;
	}
}

/* activeリストの末尾にタスクを追加する(循環リスト) */
static void activelist_add(struct schedulable **head, struct schedulable *s)
{
	struct schedulable *ins;

	CRT_ASSERT(head != NULL);
	CRT_ASSERT(s != NULL);

	/* リストが空の場合、リストの先頭にセットする */
	if(*head == NULL) {
		*head	= s;
		s->next = s;	/* 先頭に循環させる */
		return;
	}

	/* 末尾を探して末尾に追加する */
	ins = *head;
	while(ins->next != *head)	/* 循環リストなのでnextが*headなら末尾 */
		ins = ins->next;
	ins->next = s;		/* 挿入する */
	s->next   = *head;	/* 先頭に循環させる */
}

/* activeリストからタスクを削除する */
static void activelist_del(struct schedulable **head, struct schedulable *s)
{
	struct schedulable *prev;

	CRT_ASSERT(head != NULL);
	CRT_ASSERT(*head != NULL);
	CRT_ASSERT(s != NULL);

	if(s == s->next) {
		/* リストにノードが１つしかない場合 */
		*head = NULL;
		s->next = NULL;
		return;
	} else {
		/* １つ前のノードを探す */
		prev = *head;
		while(prev->next != s)
			prev = prev->next;

		/* リンクリストからはずす */
		prev->next = s->next;

		/* sが先頭だった場合、先頭を変更する */
		if(s == *head)
			*head = s->next;
	}
}

/* activeリストの先頭を1つ進めて回転させる(循環リスト) */
static void activelist_turn(struct schedulable **head)
{
	*head = (*head)->next;
}

/* timewaitリストにタスクを追加する */
static void timewaitlist_add(struct schedulable **head, struct schedulable *s)
{
	CRT_ASSERT(head != NULL);
	CRT_ASSERT(s != NULL);

	/* 待機終了時刻とプライオリティから挿入位置を求める */
	struct schedulable *search = *head, *prev = NULL;
	while(search != NULL) {
		if(search->timeout > s->timeout)
			break;
		else if((search->timeout == s->timeout)
				&& (search->priority <= s->priority))
			break;
		prev   = search;
		search = search->next;
	}

	/* 挿入する */
	if(prev == NULL) {
		s->next = *head;
		*head	= s;
	} else {
		s->next    = prev->next;
		prev->next = s;
	}
}

/* timewaitリストからタスクを削除する */
static void timewaitlist_del(struct schedulable **head, struct schedulable *s)
{
	struct schedulable *prev;

	CRT_ASSERT(head != NULL);
	CRT_ASSERT(*head != NULL);
	CRT_ASSERT(s != NULL);

	/* sをリンクリストから切り離す */
	if(*head == s) {
		*head = s->next;	/* sが先頭ノードである場合 */
		s->next = NULL;
	} else {
		/* 1つ前のノードを探しす */
		prev = *head;
		while(prev->next != s)
			prev = prev->next;

		/* sをリンクリストから切り離す */
		prev->next = s->next;
	}
}
