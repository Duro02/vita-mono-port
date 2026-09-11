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

/* 常开 STW 诊断 (不受 vita_trace_to_file 开关影响, 只记稀有事件). */
static void
stw_log (const char *tag, long a, long b)
{
	char tbuf [96];
	SceUID fd = sceIoOpen (SHIM_TRACE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
	if (fd < 0)
		return;
	{
		int n = snprintf (tbuf, sizeof (tbuf), "%s(%lx,%lx)\n", tag, a, b);
		if (n > 0)
			sceIoWrite (fd, tbuf, n);
	}
	sceIoClose (fd);
}

static void
shim_tracef (const char *func, long a, long b, long c, long ret)
{
	char tbuf [160];
	SceUID fd;
	extern int vita_trace_to_file;
	if (!vita_trace_to_file)
		return;
	fd = sceIoOpen (SHIM_TRACE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
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
	SceUID fd;
	extern int vita_trace_to_file;
	(void)msg;
	if (!vita_trace_to_file)
		return;
	fd = sceIoOpen (SHIM_TRACE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
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

/* 前向声明 (定义在 mmap 之后) */
static void *vita_freerange_take_locked (size_t len);
static void vita_mmap_fill (void *base, size_t len, int fd, off_t offset);

/* 前向声明 (定义在文件路径 wraps 区) */
static unsigned long vita_ino_of_path (const char *path);
static void vita_fd_record_locked (int fd, unsigned long ino);
static unsigned long vita_fd_ino_locked (int fd);

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

	/* 先复用 munmap 攒下的空闲区间 (地址原地保留, 必须清零) */
	pthread_mutex_lock (&vita_map_lock);
	{
		void *reuse = vita_freerange_take_locked (len);
		if (reuse) {
			pthread_mutex_unlock (&vita_map_lock);
			memset (reuse, 0, len);
			if (fd >= 0) {
				vita_mmap_fill (reuse, len, fd, offset);
				shim_tracef ("mmapREUSE-FILE", (long) len, (long) fd, (long) offset, (long) reuse);
			} else {
				shim_tracef ("mmapREUSE", (long) len, (long) prot, (long) flags, (long) reuse);
			}
			return reuse;
		}
	}
	pthread_mutex_unlock (&vita_map_lock);

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

	/* 文件映射: 把文件内容读进来 (mono 用它加载程序集!) */
	if (fd >= 0) {
		vita_mmap_fill (base, len, fd, offset);
		shim_tracef ("mmapFILE", (long) len, (long) fd, (long) offset, (long) base);
	}

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

/* 空闲子区间 (munmap 攒下, mmap 复用).
 * Vita 拆不了 memblock: 保留内存必须留在原地址, 所以部分 munmap 只标记,
 * 不还内核. memblock 只有整块命中时才真正释放. */
struct vita_freerange {
	void   *base;
	size_t  size;
	struct vita_freerange *next;
};

static struct vita_freerange *vita_freeranges = NULL;

/* 有序插入 + 全链合并相邻区间. 调用时已持有 vita_map_lock. */
static void
vita_freerange_add_locked (void *addr, size_t len)
{
	struct vita_freerange *r, **pp, *cur, *next;

	if (len == 0)
		return;
	r = (struct vita_freerange *)malloc (sizeof (*r));
	if (!r)
		return;
	r->base = addr;
	r->size = len;
	pp = &vita_freeranges;
	while (*pp && (*pp)->base < addr)
		pp = &(*pp)->next;
	r->next = *pp;
	*pp = r;
	/* 一次遍历合并所有相邻/重叠 */
	cur = vita_freeranges;
	while (cur && cur->next) {
		char *cend = (char *)cur->base + cur->size;
		next = cur->next;
		if (cend >= (char *)next->base) {
			char *nend = (char *)next->base + next->size;
			if (nend > cend)
				cur->size = nend - (char *)cur->base;
			cur->next = next->next;
			free (next);
		} else {
			cur = next;
		}
	}
}

/* first-fit 取一块 >= len 的空闲区间 (精确 carve, 剩余挂回).
 * 调用时已持有 vita_map_lock. 成功返回基址, 失败返回 NULL. */
static void *
vita_freerange_take_locked (size_t len)
{
	struct vita_freerange **pp = &vita_freeranges;
	while (*pp) {
		struct vita_freerange *r = *pp;
		if (r->size >= len) {
			void *base = r->base;
			if (r->size == len) {
				*pp = r->next;
				free (r);
			} else {
				r->base = (char *)r->base + len;
				r->size -= len;
			}
			return base;
		}
		pp = &r->next;
	}
	return NULL;
}

/* 文件内容填入映射 (程序集加载用). 不加锁 (纯 IO). */
static void
vita_mmap_fill (void *base, size_t len, int fd, off_t offset)
{
	off_t cur = lseek (fd, 0, SEEK_CUR);
	if (cur != (off_t) -1 && lseek (fd, offset, SEEK_SET) == offset) {
		size_t left = len;
		char *dst = (char *) base;
		while (left > 0) {
			ssize_t r = read (fd, dst, left);
			if (r <= 0)
				break;
			dst += r;
			left -= (size_t) r;
		}
		lseek (fd, cur, SEEK_SET);
	}
}

int
munmap (void *addr, size_t len)
{
	shim_trace ("munmap");
	stw_log ("munmap-enter", (long) addr, (long) len);
	struct vita_mapping *m, **prev;
	int ret = 0;

	pthread_mutex_lock (&vita_map_lock);
	prev = &vita_mappings;
	for (m = vita_mappings; m; prev = &m->next, m = m->next) {
		if ((const char *)addr >= (const char *)m->base &&
		    (const char *)addr + len <= (const char *)m->base + m->size)
			break;
	}
	if (!m) {
		pthread_mutex_unlock (&vita_map_lock);
		stw_log ("munmap-nomatch", (long) addr, (long) len);
		errno = EINVAL;
		return -1;
	}
	/* 整块命中: 真还内核. 子区间: 只记空闲链表 (地址原地保留,
	 * 后续 mmap 复用; Vita 拆不了 memblock, 搬迁会悬空调用方指针). */
	{
		char *base = (char *) m->base;
		if ((char *)addr == base && len == m->size) {
			*prev = m->next;
			/* 顺带 purge 与这块有任何重叠的空闲项 (块已还, 复用即踩;
			 * 注意空闲项可能跨 mapping 合并过, 不能只看起点). */
			{
				struct vita_freerange **fp = &vita_freeranges;
				char *bend = base + m->size;
				while (*fp) {
					char *fb = (char *)(*fp)->base;
					char *fend = fb + (*fp)->size;
					if (fb < bend && fend > base) {
						struct vita_freerange *dead = *fp;
						*fp = dead->next;
						free (dead);
					} else {
						fp = &(*fp)->next;
					}
				}
			}
			pthread_mutex_unlock (&vita_map_lock);
			stw_log ("munmap-freeing", (long) m->uid, (long) m->size);
			if (sceKernelFreeMemBlock (m->uid) < 0)
				ret = -1;
			stw_log ("munmap-exit", (long) ret, 0);
			free (m);
			return ret;
		}
		vita_freerange_add_locked (addr, len);
		pthread_mutex_unlock (&vita_map_lock);
		stw_log ("munmap-partial", (long) addr, (long) len);
		return 0;
	}
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

/* Mono 把 "ux0:/..." 当相对路径, 跟 cwd ("app0:/") 反复拼接:
 * app0:/ux0:/x, 甚至 app0:/app0:/ux0:/x. 这里剥掉多余的 app0:/ 前缀,
 * 露出真正的设备路径. 返回新 malloc 的串 (调用方用完 free), 不必剥
 * 原样返回 path 本身 (不 free!). */
static const char *
vita_fs_normalize (const char *path, int *owned)
{
	const char *p = path;
	*owned = 0;
	if (!p)
		return p;
	for (;;) {
		const char *rest;
		const char *colon;
		const char *slash;
		if (strncmp (p, "app0:/", 6) != 0)
			break;
		rest = p + 6;
		if (strncmp (rest, "app0:/", 6) == 0) {
			p = rest;
			continue;
		}
		/* 下一段冒号在斜杠前 => 另一个设备 (ux0:), 剥掉本层 */
		colon = strchr (rest, ':');
		slash = strchr (rest, '/');
		if (colon && (!slash || colon < slash)) {
			p = rest;
			continue;
		}
		break;
	}
	if (p != path) {
		char *n = strdup (p);
		if (n) {
			*owned = 1;
			return n;
		}
	}
	return path;
}

int
__wrap_open (const char *path, int flags, ...)
{
	extern int __real_open (const char *path, int flags, ...);
	int mode = 0;
	int ret;
	int owned = 0;
	if (flags & 0x40 /*O_CREAT*/) {
		__builtin_va_list ap;
		__builtin_va_start (ap, flags);
		mode = __builtin_va_arg (ap, int);
		__builtin_va_end (ap);
	}
	path = vita_fs_normalize (path, &owned);
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
	if (ret >= 0) {
		pthread_mutex_lock (&vita_map_lock);
		vita_fd_record_locked (ret, vita_ino_of_path (path));
		pthread_mutex_unlock (&vita_map_lock);
	}
	if (owned)
		free ((void *) path);
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

/* st_ino 合成: newlib-vita 的 stat 给不出有效 ino, Mono 拿
 * (st_dev,st_ino) 做 share 冲突判定, 常量会导致所有文件互斥
 * ("Sharing violation"). 按规范化路径哈希合成稳定 ino;
 * fstat 走 fd->ino 表. */
static unsigned long
vita_ino_of_path (const char *path)
{
	unsigned long h = 5381;
	unsigned char c;
	if (!path)
		return 1;
	while ((c = (unsigned char) *path++) != 0)
		h = h * 33 + c;
	return h ? h : 1;
}

#define VITA_FDMAP 64
static struct {
	int fd;
	unsigned long ino;
} vita_fds [VITA_FDMAP];
static int vita_fds_used = 0;

static void
vita_fd_record_locked (int fd, unsigned long ino)
{
	int i;
	for (i = 0; i < vita_fds_used; i++) {
		if (vita_fds [i].fd == fd) {
			vita_fds [i].ino = ino;
			return;
		}
	}
	if (vita_fds_used < VITA_FDMAP) {
		vita_fds [vita_fds_used].fd = fd;
		vita_fds [vita_fds_used].ino = ino;
		vita_fds_used++;
	}
}

static unsigned long
vita_fd_ino_locked (int fd)
{
	int i;
	for (i = 0; i < vita_fds_used; i++) {
		if (vita_fds [i].fd == fd)
			return vita_fds [i].ino;
	}
	return (unsigned long) (fd + 16);
}

/* 文件路径类 wrap: 统一过归一化 (见 vita_fs_normalize) */
int
__wrap_stat (const char *path, struct stat *buf)
{
	extern int __real_stat (const char *path, struct stat *buf);
	int owned = 0, ret;
	path = vita_fs_normalize (path, &owned);
	ret = __real_stat (path, buf);
	if (ret == 0 && buf) {
		buf->st_dev = 1;
		buf->st_ino = vita_ino_of_path (path);
	}
	if (owned)
		free ((void *) path);
	return ret;
}

int
__wrap_fstat (int fd, struct stat *buf)
{
	extern int __real_fstat (int fd, struct stat *buf);
	int ret = __real_fstat (fd, buf);
	if (ret == 0 && buf) {
		unsigned long ino;
		buf->st_dev = 1;
		pthread_mutex_lock (&vita_map_lock);
		ino = vita_fd_ino_locked (fd);
		pthread_mutex_unlock (&vita_map_lock);
		buf->st_ino = ino;
	}
	return ret;
}

int
__wrap_mkdir (const char *path, mode_t mode)
{
	extern int __real_mkdir (const char *path, mode_t mode);
	int owned = 0, ret;
	path = vita_fs_normalize (path, &owned);
	ret = __real_mkdir (path, mode);
	if (owned)
		free ((void *) path);
	return ret;
}

int
__wrap_rmdir (const char *path)
{
	extern int __real_rmdir (const char *path);
	int owned = 0, ret;
	path = vita_fs_normalize (path, &owned);
	ret = __real_rmdir (path);
	if (owned)
		free ((void *) path);
	return ret;
}

int
__wrap_unlink (const char *path)
{
	extern int __real_unlink (const char *path);
	int owned = 0, ret;
	path = vita_fs_normalize (path, &owned);
	ret = __real_unlink (path);
	if (owned)
		free ((void *) path);
	return ret;
}

int
__wrap_rename (const char *oldpath, const char *newpath)
{
	extern int __real_rename (const char *oldpath, const char *newpath);
	int owned1 = 0, owned2 = 0, ret;
	oldpath = vita_fs_normalize (oldpath, &owned1);
	newpath = vita_fs_normalize (newpath, &owned2);
	ret = __real_rename (oldpath, newpath);
	if (owned1)
		free ((void *) oldpath);
	if (owned2)
		free ((void *) newpath);
	return ret;
}

#include <dirent.h>

/* 目录枚举原生实现: newlib-vita 的 readdir 在 glob 循环里不终止
 * ( Battery T08 / GetFiles 实锤 ), 直接走 sceIoDopen/Dread.
 * DIR 对调用方不透明 (eglib 只透传指针), 用自有结构. */
struct vita_dir {
	SceUID dfd;
	char path [256];
	struct dirent ent;
};

DIR *
__wrap_opendir (const char *path)
{
	extern DIR *__real_opendir (const char *path);
	int owned = 0;
	(void) __real_opendir;
	char pathbuf [256];
	struct vita_dir *d;
	SceUID fd;
	path = vita_fs_normalize (path, &owned);
	pathbuf [0] = 0;
	if (path)
		strncpy (pathbuf, path, sizeof (pathbuf) - 1);
	fd = sceIoDopen (path);
	if (owned)
		free ((void *) path);
	if (fd < 0) {
		errno = ENOENT;
		return NULL;
	}
	d = (struct vita_dir *) malloc (sizeof (*d));
	if (!d) {
		sceIoDclose (fd);
		errno = ENOMEM;
		return NULL;
	}
	d->dfd = fd;
	strncpy (d->path, pathbuf, sizeof (d->path) - 1);
	d->path [sizeof (d->path) - 1] = 0;
	memset (&d->ent, 0, sizeof (d->ent));
	stw_log ("opendir-ok", (long) fd, 0);
	return (DIR *) d;
}

struct dirent *
__wrap_readdir (DIR *dp)
{
	struct vita_dir *d = (struct vita_dir *) dp;
	SceIoDirent e;
	int r;
	if (!d || d->dfd < 0) {
		errno = EBADF;
		return NULL;
	}
	{
		/* VITA-TEMP: 区分单 DIR 循环 vs 多 DIR, 确认后删 */
		static int vita_readdir_call_n = 0;
		if (vita_readdir_call_n < 40)
			stw_log ("readdir-call", (long) dp, (long) d->dfd);
		vita_readdir_call_n++;
	}
	r = sceIoDread (d->dfd, &e);
	if (r <= 0) {
		static int vita_readdir_end_n = 0;
		if (vita_readdir_end_n < 200)
			stw_log ("readdir-end", (long) r, 0);
		vita_readdir_end_n++;
		return NULL;
	}
	{
		static int vita_readdir_n = 0;
		if (vita_readdir_n < 30) {
			char tbuf [300];
			SceUID fd = sceIoOpen (SHIM_TRACE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
			if (fd >= 0) {
				int n = snprintf (tbuf, sizeof (tbuf), "readdir(%d)=%s\n", vita_readdir_n, e.d_name);
				if (n > 0)
					sceIoWrite (fd, tbuf, n);
				sceIoClose (fd);
			}
			vita_readdir_n++;
		}
	}
	memset (&d->ent, 0, sizeof (d->ent));
	strncpy (d->ent.d_name, e.d_name, sizeof (d->ent.d_name) - 1);
	return &d->ent;
}

int
__wrap_closedir (DIR *dp)
{
	struct vita_dir *d = (struct vita_dir *) dp;
	if (!d) {
		errno = EBADF;
		return -1;
	}
	stw_log ("closedir", (long) d->dfd, 0);
	if (d->dfd >= 0)
		sceIoDclose (d->dfd);
	free (d);
	return 0;
}

void
__wrap_rewinddir (DIR *dp)
{
	struct vita_dir *d = (struct vita_dir *) dp;
	SceUID fd;
	if (!d)
		return;
	if (d->dfd >= 0)
		sceIoDclose (d->dfd);
	fd = sceIoDopen (d->path);
	d->dfd = fd;
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

/* ---------------- vita console/file trace ----------------
 * newlib 的 fd 1/2 不可用, 所有诊断输出走 sceIoWrite(1) (VitaShell 控制台 /
 * Vita3K pty 实时可见) 并追加到文件. */
int vita_trace_to_file = 1;

int
vita_trace_write (const char *b, int n)
{
	SceUID fd;
	sceIoWrite (1, b, n);
	if (vita_trace_to_file) {
		fd = sceIoOpen ("ux0:data/monoapp/vita-trace.log",
			SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
		if (fd >= 0) {
			sceIoWrite (fd, b, n);
			sceIoClose (fd);
		}
	}
	return n;
}

/* write(1/2) 转接到 sceIo, 修复 mono 内部日志黑洞 */
ssize_t
__wrap_write (int fd, const void *buf, size_t count)
{
	extern ssize_t __real_write (int fd, const void *buf, size_t count);
	if (fd == 1 || fd == 2)
		return sceIoWrite (fd, buf, count);
	return __real_write (fd, buf, count);
}

/* abort/raise 包装: 打印调用, 定位静默 abort */
void
__wrap_abort (void)
{
	extern void __real_abort (void);
	const char *m = "WRAP-ABORT called!\n";
	sceIoWrite (1, m, 22);
	__real_abort ();
}

int
__wrap_raise (int sig)
{
	extern int __real_raise (int sig);
	char b [64];
	int n = snprintf (b, sizeof (b), "WRAP-RAISE sig=%d\n", sig);
	if (n > 0)
		sceIoWrite (1, b, n);
	return __real_raise (sig);
}

/* ---------------- fcntl (newlib-vita 对控制台 fd 可能返回 -1) ---------------- */
#include <stdarg.h>

int
__wrap_fcntl (int fd, int cmd, ...)
{
	extern int __real_fcntl (int fd, int cmd, ...);
	va_list ap;
	long arg;
	int ret;

	va_start (ap, cmd);
	arg = va_arg (ap, long);
	va_end (ap);

	ret = __real_fcntl (fd, cmd, arg);
	if (ret != -1)
		return ret;

	/* 真实调用失败: 对标准 fd 给出合理默认值 */
	if (fd < 0)
		return -1;
	switch (cmd) {
	case F_GETFL:
		return O_RDWR;
	case F_SETFL:
		return 0;
#ifdef F_GETFD
	case F_GETFD:
		return 0;
#endif
#ifdef F_SETFD
	case F_SETFD:
		return 0;
#endif
#ifdef F_DUPFD
	case F_DUPFD:
		return dup (fd);
#endif
	/* Vita 无建议性文件锁 (单应用系统, 无跨进程竞争者): 假装加/解锁
	 * 成功, F_GETLK 报无冲突. Mono 拿它做 Windows share-mode 模拟,
	 * 失败会报 Sharing violation. */
	case F_SETLK:
	case F_SETLKW:
		return 0;
	case F_GETLK: {
		struct flock *fl = (struct flock *) arg;
		if (fl)
			fl->l_type = F_UNLCK;
		return 0;
	}
	default:
		errno = EINVAL;
		return -1;
	}
}

/* ---------------- getrusage (newlib-vita 返回 -1/EINVAL, 线程池 hill-climbing
 * 经 mono_cpu_usage 调用 g_error 直接 abort; 给全零即 "0% CPU 占用") ---------------- */
#include <sys/resource.h>

int
__wrap_getrusage (int who, struct rusage *usage)
{
	(void) who;
	if (usage)
		memset (usage, 0, sizeof (*usage));
	return 0;
}

/* ---------------- pthread_kill 仿真 + 可中断等待 ----------------
 * Vita 内核无异步信号. Mono 混合挂起用 pthread_kill 发
 * suspend/restart/abort 信号 (mono-threads-posix.c); 这里:
 *  - __wrap_pthread_kill 只把信号记到表里, 返回成功;
 *  - 所有阻塞等待 (sem/cond) 改写成 10ms 量子的定时等待,
 *    每次超时检查本线程是否有待处理信号, 有则调
 *    mono_threads_state_poll() 自挂起 (协作式收敛).
 * 语义保持: 定时等待的超时/虚假唤醒对调用方都合法 (重入等待).
 * 时钟注意: Vita 的 pte_relmillisecs 用 ftime(纪元时) 算相对超时;
 * Mono 的 cond abstime 是 MONOTONIC 基准, sem abstime 是 gettimeofday
 * 基准. 所以 cond 包必须把剩余时间换算成 realtime deadline 再调原语,
 * 否则恒为 0ms 超时 → 忙转 + 丢唤醒. sem 包直接用 realtime 量子. */
#include <semaphore.h>

#define VITA_SIGSLOTS 32
#define VITA_WAIT_QUANTUM_MS 10

static struct {
	pthread_t tid;
	int sig;
} vita_sigpending [VITA_SIGSLOTS];
static pthread_mutex_t vita_siglock = PTHREAD_MUTEX_INITIALIZER;

extern void mono_threads_state_poll (void);

/* Mono 侧打了补丁 (patches/013) 的 mono_threads_pthread_kill 调这个,
 * 把挂起/恢复信号记到表里 (__wrap_pthread_kill 是死代码: 没有
 * HAVE_PTHREAD_KILL 时 Mono 根本不调 pthread_kill). */
void
vita_note_thread_signal (pthread_t tid, int sig)
{
	int i;

	if (sig == 0)
		return;
	pthread_mutex_lock (&vita_siglock);
	for (i = 0; i < VITA_SIGSLOTS; i++) {
		if (vita_sigpending [i].sig == 0 ||
		    pthread_equal (vita_sigpending [i].tid, tid)) {
			vita_sigpending [i].tid = tid;
			vita_sigpending [i].sig = sig;
			break;
		}
	}
	pthread_mutex_unlock (&vita_siglock);
	stw_log ("vita-note-sig", (long) tid, (long) sig);
}

static void
vita_add_qms (struct timespec *ts, long ms)
{
	ts->tv_sec += ms / 1000;
	ts->tv_nsec += (ms % 1000) * 1000000L;
	if (ts->tv_nsec >= 1000000000L) {
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000L;
	}
}

static int
vita_ts_before (const struct timespec *a, const struct timespec *b)
{
	if (a->tv_sec != b->tv_sec)
		return a->tv_sec < b->tv_sec;
	return a->tv_nsec < b->tv_nsec;
}

/* 有待处理信号则消费并自挂起轮询; 其它线程零开销 (一次查表). */
static void
vita_maybe_self_suspend (void)
{
	pthread_t self = pthread_self ();
	int i, sig = 0;

	pthread_mutex_lock (&vita_siglock);
	for (i = 0; i < VITA_SIGSLOTS; i++) {
		if (vita_sigpending [i].sig != 0 &&
		    pthread_equal (vita_sigpending [i].tid, self)) {
			sig = vita_sigpending [i].sig;
			vita_sigpending [i].sig = 0;
			break;
		}
	}
	pthread_mutex_unlock (&vita_siglock);
	if (sig != 0) {
		stw_log ("vita-self-suspend", (long) self, (long) sig);
		mono_threads_state_poll ();
		stw_log ("vita-resumed", (long) self, (long) sig);
	}
}

int
__wrap_pthread_kill (pthread_t thread, int sig)
{
	int i;

	if (sig == 0)
		return 0;
	pthread_mutex_lock (&vita_siglock);
	for (i = 0; i < VITA_SIGSLOTS; i++) {
		if (vita_sigpending [i].sig == 0 ||
		    pthread_equal (vita_sigpending [i].tid, thread)) {
			vita_sigpending [i].tid = thread;
			vita_sigpending [i].sig = sig;
			break;
		}
	}
	pthread_mutex_unlock (&vita_siglock);
	return 0;
}

/* Vita 的 pte_relmillisecs 用 ftime (纪元时) 算相对超时, 而 Mono 的
 * cond abstime 是 CLOCK_MONOTONIC 基准 (setclock 指定) —— 直接传会恒
 * 为负被钳成 0ms, 定时等待秒回、 Lost wakeup、忙转. 这里把剩余时间
 * 换算成 realtime deadline 再调原语, 原语才能真正 park 住、信号能唤醒.
 * (sem 那边 abstime 本来就是 gettimeofday/纪元时, 不用换.) */
static void
vita_mono_deadline_to_realtime (const struct timespec *mono_abstime,
	long quantum_ms, struct timespec *real_dl)
{
	struct timespec mn, rt;
	long rem_ms;

	clock_gettime (CLOCK_MONOTONIC, &mn);
	rem_ms = (mono_abstime->tv_sec - mn.tv_sec) * 1000L +
		(mono_abstime->tv_nsec - mn.tv_nsec) / 1000000L;
	if (rem_ms < 0)
		rem_ms = 0;
	if (rem_ms > quantum_ms)
		rem_ms = quantum_ms;
	clock_gettime (CLOCK_REALTIME, &rt);
	*real_dl = rt;
	vita_add_qms (real_dl, rem_ms);
}

int
__wrap_pthread_cond_wait (pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	extern int __real_pthread_cond_timedwait (pthread_cond_t *,
		pthread_mutex_t *, const struct timespec *);
	/* 无限等: 60s realtime 量子循环. 不调 state_poll: 调用方
	 * (Mono 自家的 coop 层) 本来就包了 GC_SAFE, parked 线程是
	 * BLOCKING 态, STW 天生跳过; 轮询反而会在禁窗期炸 assert. */
	for (;;) {
		struct timespec dl, rt;
		int r;
		clock_gettime (CLOCK_REALTIME, &rt);
		dl = rt;
		vita_add_qms (&dl, 60000);
		r = __real_pthread_cond_timedwait (cond, mutex, &dl);
		if (r == 0)
			return 0;
		if (r != ETIMEDOUT)
			return r;
	}
}

int
__wrap_pthread_cond_timedwait (pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime)
{
	extern int __real_pthread_cond_timedwait (pthread_cond_t *,
		pthread_mutex_t *, const struct timespec *);
	struct timespec dl;
	/* 单次调用, POSIX 精确语义: 换算后原语真 park, 信号正常唤醒. */
	vita_mono_deadline_to_realtime (abstime, 60000, &dl);
	return __real_pthread_cond_timedwait (cond, mutex, &dl);
}
