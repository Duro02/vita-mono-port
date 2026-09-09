/*
 * Vita sys/un.h shim - AF_UNIX 地址结构.
 * Vita 网络栈不支持 AF_UNIX (attach 功能运行时不可用, 但可编译).
 */
#ifndef _VITA_SHIM_SYS_UN_H
#define _VITA_SHIM_SYS_UN_H

#include <sys/socket.h>

#define UNIX_PATH_MAX 108

struct sockaddr_un {
	unsigned short sun_family;  /* AF_UNIX */
	char sun_path[UNIX_PATH_MAX];
};

#endif /* _VITA_SHIM_SYS_UN_H */
