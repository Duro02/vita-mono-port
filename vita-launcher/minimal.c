/* 最小诊断版: 只写日志+睡眠, 排除 vita2d/mono */
#include <stdio.h>
#include <string.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>

#define LOG_PATH "ux0:data/monoapp/launcher.log"

static void
log_write (const char *msg)
{
	SceUID fd = sceIoOpen (LOG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
	if (fd >= 0) {
		sceIoWrite (fd, msg, strlen (msg));
		sceIoClose (fd);
	}
}

int
main (void)
{
	log_write ("MINIMAL: entry\n");
	sceKernelDelayThread (3 * 1000 * 1000);
	log_write ("MINIMAL: after 3s sleep\n");
	sceKernelExitProcess (0);
	return 0;
}
