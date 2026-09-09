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
	/* v1: 不提供精确栈边界, mono 侧有 NULL 容错路径 */
	*staddr = NULL;
	*stsize = 0;
}

gboolean
mono_threads_platform_is_main_thread (void)
{
	return pthread_equal (pthread_self (), vita_main_thread);
}

void
mono_threads_platform_init (void)
{
}

gboolean
mono_threads_platform_in_critical_region (THREAD_INFO_TYPE *info)
{
	return FALSE;
}

gboolean
mono_threads_platform_yield (void)
{
	sched_yield ();
	return TRUE;
}

void
mono_threads_platform_exit (gsize exit_code)
{
	pthread_exit ((void *) exit_code);
}

guint64
mono_native_thread_os_id_get (void)
{
	return (guint64) (uintptr_t) pthread_self ();
}

#endif /* defined(__vita__) */
