/**
 * \file
 * PS Vita 线程平台后端.
 *
 * 基于 VitaSDK 的 pthread (ScePthread 包装). 提供 mono-threads.h
 * 要求的平台钩子. 栈边界 v1 返回 NULL (mono 容忍, GC 用保守回退),
 * 后续版本用包装 pthread_create 精确记录.
 */

#include <config.h>

#if defined(__vita__)

#include <mono/utils/mono-threads.h>
#include <pthread.h>
#include <sched.h>
#include <psp2/kernel/threadmgr.h>

static pthread_t vita_main_thread;

__attribute__((constructor))
static void
vita_record_main_thread (void)
{
	vita_main_thread = pthread_self ();
}

void
mono_threads_platform_get_stack_bounds (guint8 **staddr, size_t *stsize)
{
	SceKernelThreadInfo info;
	SceUID thid;

	/* 真实栈边界: pthread-embedded 线程即 sce 内核线程 */
	thid = sceKernelGetThreadId ();
	memset (&info, 0, sizeof (info));
	info.size = sizeof (info);
	if (sceKernelGetThreadInfo (thid, &info) == 0 && info.stack && info.stackSize > 0) {
		*staddr = (guint8 *) info.stack;
		*stsize = (size_t) info.stackSize;
		return;
	}

	/* 兜底: 当前 SP 向下 1MB (总比 NULL 强, register_thread 会 assert) */
	{
		guint8 *sp = (guint8 *) __builtin_frame_address (0);
		*staddr = sp - (1 * 1024 * 1024);
		*stsize = (size_t) (1 * 1024 * 1024);
	}
}

gboolean
mono_threads_platform_is_main_thread (void)
{
	return pthread_equal (pthread_self (), vita_main_thread);
}

guint64
mono_native_thread_os_id_get (void)
{
	return (guint64) (uintptr_t) pthread_self ();
}

#endif /* defined(__vita__) */
