/*
 * Vita netinet/in.h 垫片.
 * Vita SDK 无 IPv6 socket 选项, 这里补 Linux 标准数值;
 * 运行时内核可能返回 ENOPROTOOPT, 由上层 (MonoGame/Lidgren) 容错.
 */
#ifndef _VITA_SHIM_NETINET_IN_H
#define _VITA_SHIM_NETINET_IN_H

#include_next <netinet/in.h>

#define IPV6_UNICAST_HOPS   16
#define IPV6_MULTICAST_IF   17
#define IPV6_MULTICAST_HOPS 18
#define IPV6_MULTICAST_LOOP 19
#define IPV6_JOIN_GROUP     20
#define IPV6_LEAVE_GROUP    21

struct ipv6_mreq {
	struct in6_addr ipv6mr_multiaddr;
	unsigned        ipv6mr_interface;
};

#endif /* _VITA_SHIM_NETINET_IN_H */
