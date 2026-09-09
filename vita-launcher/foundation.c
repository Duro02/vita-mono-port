/* 地基测试: 验证 mono 依赖的 newlib/Vita 原语是否真实可用.
 * 每个测试独立写日志(malloc前/后), 崩了也能定位到行. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>

#define LOG_PATH "ux0:data/monoapp/foundation.log"

static void
log_write (const char *msg)
{
	SceUID fd = sceIoOpen (LOG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
	if (fd >= 0) {
		sceIoWrite (fd, msg, strlen (msg));
		sceIoClose (fd);
	}
}

static char buf [256];

#define T(name, expr) do { \
	snprintf (buf, sizeof (buf), "T %s => %ld (errno=%d)\n", name, (long)(expr), errno); \
	log_write (buf); \
} while (0)

static pthread_key_t fkey;

int
main (void)
{
	long v;

	log_write ("FOUNDATION: entry\n");

	/* 1. sysconf */
	errno = 0;
	v = sysconf (_SC_PAGESIZE);
	T ("sysconf(PAGESIZE)", v);
	errno = 0;
	v = sysconf (_SC_NPROCESSORS_ONLN);
	T ("sysconf(NPROC)", v);
	errno = 0;
	v = sysconf (_SC_PHYS_PAGES);
	T ("sysconf(PHYSPAGES)", v);

	/* 2. pthread TLS key */
	errno = 0;
	T ("pthread_key_create", pthread_key_create (&fkey, NULL));
	errno = 0;
	T ("pthread_setspecific", pthread_setspecific (fkey, (void *) 0x1234));
	errno = 0;
	snprintf (buf, sizeof (buf), "T pthread_getspecific => %p\n", pthread_getspecific (fkey));
	log_write (buf);
	pthread_key_delete (fkey);

	/* 3. 内存块 */
	{
		SceUID uid;
		void *base = NULL;
		log_write ("T sceKernelAllocMemBlock(8MB) ...\n");
		uid = sceKernelAllocMemBlock ("ftest", 0x0C20D060, 8 * 1024 * 1024, NULL);
		snprintf (buf, sizeof (buf), "  uid=0x%x\n", uid);
		log_write (buf);
		if (uid >= 0) {
			T ("sceKernelGetMemBlockBase", sceKernelGetMemBlockBase (uid, &base));
			snprintf (buf, sizeof (buf), "  base=%p\n", base);
			log_write (buf);
			if (base) {
				memset (base, 0xAB, 4096);
				T ("memset 4K + readback", ((unsigned char *) base) [100] == 0xAB ? 1 : 0);
			}
			T ("sceKernelFreeMemBlock", sceKernelFreeMemBlock (uid));
		}
	}

	/* 4. signal/raise */
	{
		log_write ("T signal(SIGUSR1, handler) ...\n");
		_sig_func_ptr old = signal (18, (_sig_func_ptr) 1 /*SIG_IGN*/);
		snprintf (buf, sizeof (buf), "  old=%p\n", old);
		log_write (buf);
		errno = 0;
		T ("raise(SIGUSR1 ignored)", raise (18));
	}

	/* 5. setenv/getenv */
	errno = 0;
	T ("setenv", setenv ("MONO_ENV_OPTIONS", "--interpreter", 1));
	{
		const char *e = getenv ("MONO_ENV_OPTIONS");
		snprintf (buf, sizeof (buf), "T getenv => %s\n", e ? e : "(null)");
		log_write (buf);
	}

	/* 6. environ 全局量 */
	{
		extern char **environ;
		snprintf (buf, sizeof (buf), "T environ=%p\n", (void *) environ);
		log_write (buf);
	}

	log_write ("FOUNDATION: all done\n");
	sceKernelExitProcess (0);
	return 0;
}
