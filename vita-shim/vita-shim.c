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
#include <psp2/io/fcntl.h>

#define SHIM_TRACE_PATH "ux0:data/monoapp/shim-trace.log"


static void
shim_tracef (const char *func, long a, long b, long c, long ret)
{
	char tbuf [160];
	SceUID fd = sceIoOpen (SHIM_TRACE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
	if (fd < 0)
		return;
	{
		int n = snprintf (tbuf, sizeof (tbuf), "%s(%lx,%lx,%lx)=%lx\n", func, a, b, c, ret);
		if (n > 0)
			sceIoWrite (fd, tbuf, n);
	}
	sceIoClose (fd);
}

static void
shim_trace (const char *msg)
{
	char tbuf [256];
	SceUID fd = sceIoOpen (SHIM_TRACE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
	if (fd < 0)
		return;
	/* 手工组装, 避免 snprintf %p 依赖 */
	{
		int n = 0;
		const char *s = msg;
		while (*s && n < (int) sizeof (tbuf) - 2)
			tbuf [n++] = *s++;
		tbuf [n++] = '\n';
		sceIoWrite (fd, tbuf, n);
	}
	sceIoClose (fd);
}


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
	shim_trace ("page_round");
	return (v + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static struct vita_mapping *
vita_map_find (const void *addr)
{
	shim_trace ("vita_map_find");
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
	shim_trace ("mmap");
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
		shim_tracef ("mmapFAIL", (long) len, (long) prot, (long) flags, 0);
		return MAP_FAILED;
	}

	memset (base, 0, alen);
	shim_tracef ("mmapOK", (long) len, (long) prot, (long) flags, (long) base);

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
	shim_trace ("munmap");
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
	shim_trace ("mprotect");
	/* Vita 的权限在分配时确定, 无法动态改.
	 * RW 数据块上的 EXEC 请求先放行(将来 JIT 走 VM domain). */
	shim_tracef ("mprotect", (long) addr, (long) len, (long) prot, 0);
	(void)addr; (void)len; (void)prot;
	return 0;
}

int
msync (void *addr, size_t len, int flags)
{
	shim_trace ("msync");
	(void)addr; (void)len; (void)flags;
	return 0;
}

int
madvise (void *addr, size_t len, int advice)
{
	shim_trace ("madvise");
	(void)addr; (void)len; (void)advice;
	return 0;
}

int
mlock (const void *addr, size_t len)
{
	shim_trace ("mlock");
	(void)addr; (void)len;
	return 0;
}

int
munlock (const void *addr, size_t len)
{
	shim_trace ("munlock");
	(void)addr; (void)len;
	return 0;
}

/* ---------------- sched / process stubs ---------------- */

int
sched_yield (void)
{
	shim_trace ("sched_yield");
	sceKernelDelayThread (0);
	return 0;
}

pid_t
waitpid (pid_t pid, int *status, int options)
{
	shim_trace ("waitpid");
	(void)pid; (void)options;
	if (status)
		*status = 0;
	errno = ECHILD;
	return -1;
}

int
dup2 (int oldfd, int newfd)
{
	shim_trace ("dup2");
	(void)oldfd; (void)newfd;
	errno = ENOSYS;
	return -1;
}

int
symlink (const char *oldpath, const char *newpath)
{
	shim_trace ("symlink");
	(void)oldpath; (void)newpath;
	errno = ENOSYS;
	return -1;
}

int
sigaltstack (const stack_t *ss, stack_t *old_ss)
{
	shim_trace ("sigaltstack");
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
	shim_trace ("pthread_getattr_np");
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
	shim_trace ("dlopen");
	(void)filename; (void)flag;
	return NULL;
}

void *
dlsym (void *handle, const char *symbol)
{
	shim_trace ("dlsym");
	(void)handle; (void)symbol;
	return NULL;
}

int
dlclose (void *handle)
{
	shim_trace ("dlclose");
	(void)handle;
	return -1;
}

char *
dlerror (void)
{
	shim_trace ("dlerror");
	return (char *) "dynamic loading not supported on Vita";
}

/* ---------------- syslog stubs ---------------- */

void
openlog (const char *ident, int logopt, int facility)
{
	shim_trace ("openlog");
	(void)ident; (void)logopt; (void)facility;
}

void
syslog (int priority, const char *format, ...)
{
	shim_trace ("syslog");
	(void)priority; (void)format;
}

void
closelog (void)
{
	shim_trace ("closelog");
}

/* ---------------- termios stubs ---------------- */

#include <termios.h>

int
tcgetattr (int fd, struct termios *termios_p)
{
	shim_trace ("tcgetattr");
	(void)fd;
	if (termios_p)
		memset (termios_p, 0, sizeof (*termios_p));
	return 0;
}

int
tcsetattr (int fd, int optional_actions, const struct termios *termios_p)
{
	shim_trace ("tcsetattr");
	(void)fd; (void)optional_actions; (void)termios_p;
	return 0;
}

int
ioctl (int fd, unsigned long request, ...)
{
	shim_trace ("ioctl");
	(void)fd; (void)request;
	return 0;
}

int
tcflush (int fd, int queue_selector)
{
	shim_trace ("tcflush");
	(void)fd; (void)queue_selector;
	return 0;
}

/* ---------------- misc stubs for mono link ---------------- */

struct passwd {
	char *pw_name;
	char *pw_passwd;
	unsigned int pw_uid;
	unsigned int pw_gid;
	char *pw_gecos;
	char *pw_dir;
	char *pw_shell;
};

struct passwd *
getpwnam (const char *name)
{
	shim_trace ("getpwnam");
	(void)name;
	return NULL;
}

struct passwd *
getpwuid (unsigned int uid)
{
	shim_trace ("getpwuid");
	(void)uid;
	return NULL;
}

int
eg_getdtablesize (void)
{
	shim_trace ("eg_getdtablesize");
	return 64;
}

int
monoeg_g_spawn_async_with_pipes (const char *working_directory,
	char **argv, char **envp, int flags,
	void *child_setup, void *user_data,
	int *child_pid, int *standard_input,
	int *standard_output, int *standard_error, int *exit_status)
{
	(void)working_directory; (void)argv; (void)envp; (void)flags;
	(void)child_setup; (void)user_data; (void)child_pid;
	(void)standard_input; (void)standard_output; (void)standard_error;
	(void)exit_status;
	return 0;
}

const char *
mono_w32file_get_file_system_type (const char *path)
{
	shim_trace ("mono_w32file_get_file_system_type");
	(void)path;
	return "UNKNOWN";
}


/* ---------------- signal dispatch (newlib 缺 sigaction) ----------------
 * Vita 的信号处理走 newlib 的 signal() 表 (libc raise/kill 使用).
 * sigaction 简单转接; 掩码类调用存根. */

int
sigaction (int signo, const struct sigaction *act, struct sigaction *oldact)
{
	shim_trace ("sigaction");
	_sig_func_ptr old;
	if (oldact) {
		old = signal (signo, SIG_DFL);
		oldact->sa_handler = old;
		signal (signo, old);
		memset (&oldact->sa_mask, 0, sizeof (oldact->sa_mask));
		oldact->sa_flags = 0;
	}
	if (act) {
		if (act->sa_flags & SA_SIGINFO) {
			/* 无 SA_SIGINFO 支持; handler 签名不匹配只能尽力 */
			signal (signo, (_sig_func_ptr) act->sa_sigaction);
		} else {
			signal (signo, act->sa_handler);
		}
	}
	return 0;
}

int
sigprocmask (int how, const sigset_t *set, sigset_t *oldset)
{
	shim_trace ("sigprocmask");
	(void)how; (void)set;
	if (oldset)
		sigemptyset (oldset);
	return 0;
}

int
sigsuspend (const sigset_t *mask)
{
	shim_trace ("sigsuspend");
	(void)mask;
	/* 挂起 10ms 并返回 EINTR, 避免忙等 */
	sceKernelDelayThread (10 * 1000);
	errno = EINTR;
	return -1;
}

ssize_t
readlink (const char *path, char *buf, size_t bufsiz)
{
	shim_trace ("readlink");
	(void)path; (void)buf; (void)bufsiz;
	errno = EINVAL;
	return -1;
}

/* newlib 头声明了 posix_memalign 但 libc 没实现, memalign 有 */
#include <malloc.h>

int
posix_memalign (void **memptr, size_t alignment, size_t size)
{
	shim_trace ("posix_memalign");
	void *p = memalign (alignment, size);
	if (!p)
		return ENOMEM;
	*memptr = p;
	return 0;
}

int
posix_madvise (void *addr, size_t len, int advice)
{
	shim_trace ("posix_madvise");
	(void)addr; (void)len; (void)advice;
	return 0;
}

/* newlib 无 SIOCGIFCONF/getifaddrs: 返回空接口列表 */
void *
mono_get_local_interfaces (int family, int *interface_count)
{
	shim_trace ("mono_get_local_interfaces");
	(void)family;
	*interface_count = 0;
	return NULL;
}

/* ---------------- sysconf (newlib-vita 返回 -1/EINVAL) ----------------
 * 链接时用 -Wl,--wrap=sysconf 启用; 未列出的名字转交真正的 sysconf.
 * Vita PCH-1000: 4 核 Cortex-A9 (系统占用1核, 用户可用按4报告),
 * 用户内存约 256MB, 页 4KB. */
long
__wrap_sysconf (int name)
{
	extern long __real_sysconf (int name);
	long r;
	shim_trace ("__wrap_sysconf");
	switch (name) {
	case _SC_PAGESIZE:         /* == _SC_PAGE_SIZE */
		r = 4096;
		break;
	case _SC_NPROCESSORS_CONF:
	case _SC_NPROCESSORS_ONLN:
		r = 4;
		break;
	case _SC_PHYS_PAGES:
		r = (256 * 1024 * 1024) / 4096;
		break;
	case _SC_AVPHYS_PAGES:
		r = (128 * 1024 * 1024) / 4096;
		break;
	case _SC_OPEN_MAX:
		r = 1024;
		break;
	case _SC_CLK_TCK:
		r = 100;
		break;
	case _SC_GETPW_R_SIZE_MAX:
		r = 1024;
		break;
	case _SC_GETGR_R_SIZE_MAX:
		r = 1024;
		break;
	default:
		r = __real_sysconf (name);
		break;
	}
	shim_tracef ("sysconf", (long) name, 0, 0, r);
	return r;
}

/* ---------------- traced wraps (链接 -Wl,--wrap=...) ---------------- */

int
__wrap_open (const char *path, int flags, ...)
{
	extern int __real_open (const char *path, int flags, ...);
	int mode = 0;
	int ret;
	if (flags & 0x40 /*O_CREAT*/) {
		__builtin_va_list ap;
		__builtin_va_start (ap, flags);
		mode = __builtin_va_arg (ap, int);
		__builtin_va_end (ap);
	}
	{
		char tbuf [128];
		SceUID fd = sceIoOpen (SHIM_TRACE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
		if (fd >= 0) {
			int n = snprintf (tbuf, sizeof (tbuf), "open(%s,%x)=?\n", path ? path : "(null)", flags);
			if (n > 0)
				sceIoWrite (fd, tbuf, n);
			sceIoClose (fd);
		}
	}
	ret = (flags & 0x40) ? __real_open (path, flags, mode) : __real_open (path, flags);
	{
		char tbuf [64];
		SceUID fd = sceIoOpen (SHIM_TRACE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
		if (fd >= 0) {
			int n = snprintf (tbuf, sizeof (tbuf), "open->%d\n", ret);
			if (n > 0)
				sceIoWrite (fd, tbuf, n);
			sceIoClose (fd);
		}
	}
	return ret;
}

int
__wrap_close (int fd)
{
	extern int __real_close (int fd);
	shim_tracef ("close", (long) fd, 0, 0, 0);
	return __real_close (fd);
}

int
__wrap_pthread_create (pthread_t *thread, const pthread_attr_t *attr,
	void *(*start) (void *), void *arg)
{
	extern int __real_pthread_create (pthread_t *, const pthread_attr_t *,
		void *(*) (void *), void *);
	int ret;
	shim_tracef ("pthread_create_enter", (long) start, (long) arg, 0, 0);
	ret = __real_pthread_create (thread, attr, start, arg);
	shim_tracef ("pthread_create", (long) start, (long) arg, 0, (long) ret);
	return ret;
}
