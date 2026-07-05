#ifndef _GRAVITY_H_
#define _GRAVITY_H_

/*
 * basic types
 */

#define NULL			((void*)0)

typedef unsigned long	uint32;
typedef unsigned short	uint16;
typedef unsigned char	uint8;
typedef unsigned long	size_t;
typedef unsigned long	clock_t;
typedef unsigned long	off_t;


/*
 * system call
 */

/* int sys_invoke(int obj_desc, struct sys_invoke_param param); */
#define sys_invoke(obj_desc, param)						\
	do {												\
		int _obj    = (int)(obj_desc);					\
		int _param  = (int)(param);						\
		int _result = 0;								\
		__asm__ __volatile__(							\
"			movl	%0, %%eax						\n\t"\
"			movl	%1, %%ebx						\n\t"\
"			int		$0xc3							\n\t"\
			:											\
			: "g"(_obj), "g"(_param)					\
			: "%eax", "%ebx"							\
		);												\
	} while(0)											

/*  */
struct sys_invoke_param {
	/* メソッド番号 */
	unsigned long	method;

	/* 整数パラメータ */
	unsigned long	int_param[16];

	/* ポインタパラメータの数 */
	unsigned long	ptr_params_count;

	/* ポインタパラメータ */
	struct pointer_params {
		unsigned long	len;
		void			*buf;
	} ptr_param[1];
};

#endif
