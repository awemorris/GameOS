#ifndef SYS_KERN_DEVICE_H
#define SYS_KERN_DEVICE_H

struct block_device_reg_info {
	bool (*init)(void);
	void (*uninit)(void);
	bool (*write)(void *p, off_t ofs, size_t len);
};

struct block_device {
	bool (*init)(
};

struct character_device {
};

struct tty_device {
};

struct input_device {
};

struct net_device {
};

#endif
