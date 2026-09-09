/*
 * Vita mmap shim - sys/mman.h
 * newlib-vita 没有 mmap 家族, 这里用 sceKernelAllocMemBlock 实现.
 * 页大小按 Vita 的 4KB 处理.
 */
#ifndef _VITA_SHIM_SYS_MMAN_H
#define _VITA_SHIM_SYS_MMAN_H

#include <stddef.h>
#include <psp2/kernel/sysmem.h>

#define PROT_NONE       0x0
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_FAILED      ((void *) -1)

/* 与 Linux 对齐; Vita 内核页 4KB */
#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)

int mprotect (void *addr, size_t len, int prot);
void *mmap (void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int munmap (void *addr, size_t len);
int msync (void *addr, size_t len, int flags);
int madvise (void *addr, size_t len, int advice);
int posix_madvise (void *addr, size_t len, int advice);
#define POSIX_MADV_DONTNEED 0
#define POSIX_MADV_SEQUENTIAL 1
#define POSIX_MADV_RANDOM 2
#define POSIX_MADV_WILLNEED 3
#define POSIX_MADV_NORMAL 4
int mlock (const void *addr, size_t len);
int munlock (const void *addr, size_t len);

#endif /* _VITA_SHIM_SYS_MMAN_H */
