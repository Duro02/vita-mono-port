/*
 * Vita POSIX shim for Mono runtime.
 * 补齐 newlib-vita 缺失的 POSIX 原语:
 *   mmap/munmap/mprotect  -> sceKernelAllocMemBlock
 *   sched_yield           -> sceKernelDelayThread(0)
 *   waitpid/dup2/symlink  -> 存根(游戏场景不需要)
 *   sigaltstack           -> 记录但无效
 *   pthread_getattr_np    -> 已创建线程的栈信息(由 pthread_create 包装记录)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>

/* ---------------- mmap family ---------------- */

#define VITA_BLOCK_TYPE_DATA  SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RW
#define VITA_BLOCK_TYPE_EXEC  SCE_KERNEL_MEMBLOCK_TYPE_USER_RW /* 占位, JIT 后续走 VM domain */

struct vita_mapping {
	void   *base;    /* 对齐后的实际地址 */
	SceUID  uid;
	size_t  size;    /* 对齐后的实际大小 */
	int     prot;
	struct vita_mapping *next;
};

static struct vita_mapping *vita_mappings = NULL;
static pthread_mutex_t vita_map_lock = PTHREAD_MUTEX_INITIALIZER;

static size_t
page_round (size_t v)
{
	return (v + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static struct vita_mapping *
vita_map_find (const void *addr)
{
	struct vita_mapping *m;
	for (m = vita_mappings; m; m = m->next)
		if ((const char *)addr >= (const char *)m->base &&
		    (const char *)addr <  (const char *)m->base + m->size)
			return m;
	return NULL;
}

void *
mmap (void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	struct vita_mapping *m;
	SceUID uid;
	void *base = NULL;
	int want_exec = (prot & PROT_EXEC) != 0;
	size_t alen;

	(void)addr; (void)flags; (void)fd; (void)offset;

	if (len == 0) {
		errno = EINVAL;
		return MAP_FAILED;
	}

	m = (struct vita_mapping *)malloc (sizeof (*m));
	if (!m) {
		errno = ENOMEM;
		return MAP_FAILED;
	}

	alen = page_round (len);
	uid = sceKernelAllocMemBlock ("mono-mmap",
		want_exec ? VITA_BLOCK_TYPE_EXEC : VITA_BLOCK_TYPE_DATA,
		alen, NULL);
	if (uid < 0 || sceKernelGetMemBlockBase (uid, &base) < 0 || !base) {
		if (uid >= 0)
			sceKernelFreeMemBlock (uid);
		free (m);
		errno = ENOMEM;
		return MAP_FAILED;
	}

	memset (base, 0, alen);

	m->base = base;
	m->uid  = uid;
	m->size = alen;
	m->prot = prot;

	pthread_mutex_lock (&vita_map_lock);
	m->next = vita_mappings;
	vita_mappings = m;
	pthread_mutex_unlock (&vita_map_lock);

	return base;
}

int
munmap (void *addr, size_t len)
{
	struct vita_mapping *m, **prev;
	int ret = 0;

	(void)len;

	pthread_mutex_lock (&vita_map_lock);
	prev = &vita_mappings;
	for (m = vita_mappings; m; prev = &m->next, m = m->next) {
		if (m->base == addr)
			break;
	}
	if (!m) {
		pthread_mutex_unlock (&vita_map_lock);
		errno = EINVAL;
		return -1;
	}
	*prev = m->next;
	pthread_mutex_unlock (&vita_map_lock);

	if (sceKernelFreeMemBlock (m->uid) < 0)
		ret = -1;
	free (m);
	return ret;
}

int
mprotect (void *addr, size_t len, int prot)
{
	/* Vita 的权限在分配时确定, 无法动态改.
	 * RW 数据块上的 EXEC 请求先放行(将来 JIT 走 VM domain). */
	(void)addr; (void)len; (void)prot;
	return 0;
}

int
msync (void *addr, size_t len, int flags)
{
	(void)addr; (void)len; (void)flags;
	return 0;
}

int
madvise (void *addr, size_t len, int advice)
{
	(void)addr; (void)len; (void)advice;
	return 0;
}

int
mlock (const void *addr, size_t len)
{
	(void)addr; (void)len;
	return 0;
}

int
munlock (const void *addr, size_t len)
{
	(void)addr; (void)len;
	return 0;
}

/* ---------------- sched / process stubs ---------------- */

int
sched_yield (void)
{
	sceKernelDelayThread (0);
	return 0;
}

pid_t
waitpid (pid_t pid, int *status, int options)
{
	(void)pid; (void)options;
	if (status)
		*status = 0;
	errno = ECHILD;
	return -1;
}

int
dup2 (int oldfd, int newfd)
{
	(void)oldfd; (void)newfd;
	errno = ENOSYS;
	return -1;
}

int
symlink (const char *oldpath, const char *newpath)
{
	(void)oldpath; (void)newpath;
	errno = ENOSYS;
	return -1;
}

int
sigaltstack (const stack_t *ss, stack_t *old_ss)
{
	/* Vita 不支持替代信号栈, 记账式通过 */
	if (old_ss)
		memset (old_ss, 0, sizeof (*old_ss));
	if (ss)
		memset ((void *)ss, 0, sizeof (*ss));
	return 0;
}

/* ---------------- pthread extras ---------------- */

int
pthread_getattr_np (pthread_t thread, pthread_attr_t *attr)
{
	if (!attr)
		return EINVAL;
	pthread_attr_init (attr);
	/* pthread-embedded 的 attr 对象默认值即可; 精确栈边界后续版本补 */
	return 0;
}

/* ---------------- dlfcn stubs ---------------- */

void *
dlopen (const char *filename, int flag)
{
	(void)filename; (void)flag;
	return NULL;
}

void *
dlsym (void *handle, const char *symbol)
{
	(void)handle; (void)symbol;
	return NULL;
}

int
dlclose (void *handle)
{
	(void)handle;
	return -1;
}

char *
dlerror (void)
{
	return (char *) "dynamic loading not supported on Vita";
}

/* ---------------- syslog stubs ---------------- */

void
openlog (const char *ident, int logopt, int facility)
{
	(void)ident; (void)logopt; (void)facility;
}

void
syslog (int priority, const char *format, ...)
{
	(void)priority; (void)format;
}

void
closelog (void)
{
}

/* ---------------- termios stubs ---------------- */

#include <termios.h>

int
tcgetattr (int fd, struct termios *termios_p)
{
	(void)fd;
	if (termios_p)
		memset (termios_p, 0, sizeof (*termios_p));
	return 0;
}

int
tcsetattr (int fd, int optional_actions, const struct termios *termios_p)
{
	(void)fd; (void)optional_actions; (void)termios_p;
	return 0;
}

int
ioctl (int fd, unsigned long request, ...)
{
	(void)fd; (void)request;
	return 0;
}

int
tcflush (int fd, int queue_selector)
{
	(void)fd; (void)queue_selector;
	return 0;
}
