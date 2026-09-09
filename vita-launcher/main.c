/* Vita Mono launcher - M2 验证: 在 Vita 上运行 C# hello world
 *
 * 模式: 解释器 (Vita 内存页无执行权限, JIT 暂不可用;
 *       将来 JIT 走 sceKernelAllocMemBlockForVM + VM domain)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/processmgr.h>
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

int
main (void)
{
	log_write ("launcher: entry\n");

	vita2d_init ();
	log_write ("launcher: vita2d init done\n");
	vita2d_pvf *font = vita2d_load_default_pvf ();
	log_write ("launcher: font loaded\n");

	/* 解释器模式 */
	setenv ("MONO_ENV_OPTIONS", "--interpreter", 1);
	setenv ("MONO_PATH", APP_DIR, 1);

	log_write ("launcher: calling mono_jit_init\n");
	MonoDomain *domain = mono_jit_init (ASSEMBLY);
	if (!domain) {
		log_write ("launcher: mono_jit_init FAILED\n");
		vita2d_start_drawing ();
		vita2d_clear_screen ();
		vita2d_pvf_draw_text (font, 60, 200, RGBA8(255,0,0,255), 1.6f, "mono_jit_init FAILED");
		vita2d_end_drawing ();
		vita2d_swap_buffers ();
		sceKernelDelayThread (5 * 1000 * 1000);
		sceKernelExitProcess (1);
		return 1;
	}
	log_write ("launcher: runtime up, opening assembly\n");

	MonoAssembly *assembly = mono_domain_assembly_open (domain, ASSEMBLY);
	if (!assembly) {
		log_write ("launcher: assembly open FAILED\n");
		sceKernelExitProcess (2);
		return 2;
	}
	log_write ("launcher: invoking Main\n");

	mono_jit_exec (domain, assembly, 0, NULL);
	log_write ("launcher: Main returned\n");

	mono_jit_cleanup (domain);
	log_write ("launcher: done\n");
	;

	vita2d_start_drawing ();
	vita2d_clear_screen ();
	vita2d_pvf_draw_text (font, 60, 200, RGBA8(0,255,0,255), 1.6f, "C# OK - see launcher.log");
	vita2d_end_drawing ();
	vita2d_swap_buffers ();
	sceKernelDelayThread (5 * 1000 * 1000);

	vita2d_free_pvf (font);
	vita2d_fini ();
	sceKernelExitProcess (0);
	return 0;
}
