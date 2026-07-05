#ifndef _SYS_SCHED_H_
#define _SYS_SCHED_H_

#include <sys/types.h>
#include <sys/hal/task.h>


/*
 * タスクのリンク先スケジュールリスト
 */
enum SchedListType
{
	SCHED_LIST_UNLINKED = 0,	/* リンクされていない(スリープ) */
	SCHED_LIST_ACTIVE	= 1,	/* activeリスト */
	SCHED_LIST_TIMEWAIT	= 2,	/* timerリスト */
};

/*
 * タスクの優先度
 */
#define SCHED_PRIOR_LEVELS		(16)
#define SCHED_PRIOR_HIGH		(0)
#define SCHED_PRIOR_LOW			(15)


/*
 * スケジュール情報構造体
 *  o アーキテクチャ依存のtask_info構造体の先頭メンバとして配置する。
 *  o (struct task_info *)型を(struct schedulable *)型にキャストできる。
 *  o task_t型と(struct task_info *)型は相互にキャストできる。
 */
struct schedulable {
	int		cpu;		/* 所属CPU */
	int		status;		/* 所属リスト(SchedListType) */
	int		priority;	/* プライオリティ(SCHED_LIST */
	uint32	timeout;	/* タイムアウト(SCHED_STATUS_TIMEWAIT) */
	struct schedulable *next; /* 所属スケジュールリストでの次のタスク(循環リスト) */
};

/*
 * CPUごとのタスクスケジュール管理構造体
 */
struct cpu_sched_list {
	/* activeリスト(優先度ごと, 循環リスト) */
	struct schedulable	*active_head[SCHED_PRIOR_LEVELS];

	/* timewaitリスト(タイムアウト時刻と優先度でソート) */
	struct schedulable	*timewait_head;

	/* このCPU専用のアイドルタスク */
	struct schedulable	*idle_task;

	/* TODO: MPでのロードバランシング用に使用率などを追加 */
	/* ロードバランシングは独立したタスクを作ってCPUを転々とさせる？ */
};

/*
 * sched.c
 * すべて割り込み禁止区間でのみコール可能
 */
void shced_init(void);
void sched_link(task_t t, int list, int priority, int opt);
void sched_yield(void);
void sched_clock_handler(void);

#endif
