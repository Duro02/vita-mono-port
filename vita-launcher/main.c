/* Vita Mono launcher - M2 验证: 在 Vita 上运行 C# hello world
 *
 * 模式: 解释器 (Vita 内存页无执行权限, JIT 暂不可用;
 *       将来 JIT 走 sceKernelAllocMemBlockForVM + VM domain)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <vita2d.h>

#include <mini/jit.h>
#include <metadata/assembly.h>

#define APP_DIR "ux0:data/monoapp"
#define ASSEMBLY APP_DIR "/hello.exe"
#define LOG_PATH APP_DIR "/launcher.log"

static void
log_write (const char *msg)
{
	SceUID fd = sceIoOpen (LOG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
	if (fd >= 0) {
		sceIoWrite (fd, msg, strlen (msg));
		sceIoClose (fd);
	}
	sceIoWrite (1, msg, strlen (msg)); /* VitaShell 控制台 */
}

static void
draw_stage (vita2d_pvf *font, const char *msg)
{
	vita2d_start_drawing ();
	vita2d_clear_screen ();
	vita2d_pvf_draw_text (font, 60, 200, RGBA8(255,255,0,255), 1.6f, msg);
	vita2d_end_drawing ();
	vita2d_swap_buffers ();
}

/* mono 在 8MB 栈的工作线程上跑: 主线程默认栈太小, 装不下
 * mono 的类加载/解释器递归 */
#define MONO_THREAD_STACK (8 * 1024 * 1024)

static vita2d_pvf *g_font;

static void *
mono_worker (void *arg)
{
	(void)arg;
	log_write ("worker: thread started\n");

	MonoDomain *domain = mono_jit_init (ASSEMBLY);
	if (!domain) {
		log_write ("worker: mono_jit_init FAILED\n");
		return (void *) 1;
	}
	log_write ("worker: runtime up, opening assembly\n");

	MonoAssembly *assembly = mono_domain_assembly_open (domain, ASSEMBLY);
	if (!assembly) {
		log_write ("worker: assembly open FAILED\n");
		return (void *) 2;
	}
	log_write ("worker: invoking Main\n");

	mono_jit_exec (domain, assembly, 0, NULL);
	log_write ("worker: Main returned\n");

	mono_jit_cleanup (domain);
	log_write ("worker: done\n");
	return (void *) 42;
}

int
main (void)
{
	pthread_t mono_thread;
	pthread_attr_t attr;
	void *thread_ret = 0;
	int pr;

	log_write ("launcher: entry\n");

	vita2d_init ();
	log_write ("launcher: vita2d init done\n");
	vita2d_pvf *font = vita2d_load_default_pvf ();
	g_font = font;
	log_write ("launcher: font loaded\n");

	/* mono 内部日志 (stdout/stderr) 重定向到文件 */
	close (1);
	close (2);
	{
		SceUID out = sceIoOpen (APP_DIR "/mono-internal.log",
			SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
		SceUID err = sceIoOpen (APP_DIR "/mono-internal.log",
			SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
		{
			char b [64];
			snprintf (b, sizeof (b), "launcher: redirect fds out=%d err=%d\n",
				(int) out, (int) err);
			log_write (b);
		}
	}
	log_write ("launcher: stdio redirected\n");

	/* 解释器模式 + 内部日志.
	 * 注意: 不能用 MONO_PATH (Unix 按 ':' 切分, 会把 "ux0:..." 切碎);
	 * 改用 mono_set_dirs 直接指定目录 (不切分). mscorlib 路径:
	 *   ux0:data/monoapp/mono/4.5/mscorlib.dll */
	setenv ("MONO_ENV_OPTIONS", "--interpreter", 1);
	setenv ("MONO_LOG_LEVEL", "debug", 1);
	setenv ("MONO_LOG_MASK", "asm,type,gc", 1);
	mono_set_dirs (APP_DIR, APP_DIR);

	pthread_attr_init (&attr);
	pthread_attr_setstacksize (&attr, MONO_THREAD_STACK);
	draw_stage (font, "STAGE: mono worker thread...");
	log_write ("launcher: creating mono worker thread (8MB stack)\n");
	pr = pthread_create (&mono_thread, &attr, mono_worker, NULL);
	pthread_attr_destroy (&attr);
	if (pr != 0) {
		char b [64];
		snprintf (b, sizeof (b), "launcher: pthread_create FAILED %d\n", pr);
		log_write (b);
		sceKernelExitProcess (3);
		return 3;
	}
	pthread_join (mono_thread, &thread_ret);
	{
		char b [64];
		snprintf (b, sizeof (b), "launcher: worker joined ret=%p\n", thread_ret);
		log_write (b);
	}

	if (thread_ret == (void *) 42)
		draw_stage (font, "C# OK - see launcher.log");
	else
		draw_stage (font, "worker FAILED - see launcher.log");
	sceKernelDelayThread (5 * 1000 * 1000);

	vita2d_free_pvf (font);
	vita2d_fini ();
	sceKernelExitProcess (thread_ret == (void *) 42 ? 0 : 4);
	return 0;
}
