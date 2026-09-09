/*
 * Vita sys/ioctl.h shim - 存根.
 * ioctl 调用在 shim 里全部返回成功, 不做实事.
 */
#ifndef _VITA_SHIM_SYS_IOCTL_H
#define _VITA_SHIM_SYS_IOCTL_H

#define FIONREAD  0x541B
#define FIONBIO   0x5411
#define FIOASYNC  0x5452
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

int ioctl (int fd, unsigned long request, ...);

#endif /* _VITA_SHIM_SYS_IOCTL_H */
