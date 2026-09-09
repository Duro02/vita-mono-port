/*
 * Vita termios shim.
 * newlib-vita 的 <termios.h> 引用不存在的 <sys/termios.h>.
 * 提供最小 termios 结构与存根函数: Console 的 TTY 特性在 Vita 上无意义.
 */
#ifndef _VITA_SHIM_TERMIOS_H
#define _VITA_SHIM_TERMIOS_H

#include <sys/types.h>

#define NCCS 32

struct termios {
	unsigned int c_iflag;
	unsigned int c_oflag;
	unsigned int c_cflag;
	unsigned int c_lflag;
	unsigned char c_cc[NCCS];
	unsigned int c_ispeed;
	unsigned int c_ospeed;
};

#define TCSANOW    0
#define TCSADRAIN  1
#define TCSAFLUSH  2

#define IGNBRK  0x0001
#define BRKINT  0x0002
#define IGNPAR  0x0004
#define PARMRK  0x0010
#define INPCK   0x0020
#define ISTRIP  0x0040
#define INLCR   0x0100
#define IGNCR   0x0200
#define ICRNL   0x0400
#define IXON    0x0800

#define OPOST   0x0001

#define ISIG    0x0001
#define ICANON  0x0002
#define ECHO    0x0008
#define ECHONL  0x0040
#define IEXTEN  0x8000

#define VEOF    0
#define VEOL    1
#define VERASE  2
#define VKILL   3
#define VINTR   4
#define VQUIT   5
#define VMIN    6
#define VTIME   7

int tcgetattr (int fd, struct termios *termios_p);
int tcsetattr (int fd, int optional_actions, const struct termios *termios_p);

#endif /* _VITA_SHIM_TERMIOS_H */
