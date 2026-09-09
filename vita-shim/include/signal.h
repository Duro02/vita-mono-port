/*
 * Vita signal.h shim.
 * 这套 newlib 的完整 POSIX sigaction(含 SA_SIGINFO/sa_sigaction) 只存在于
 * __rtems__ 分支; Vita 走精简分支, 只有 sa_handler.
 * 兼容策略: SA_SIGINFO 伪装, sa_sigaction 映射到 sa_handler
 * (信号上下文不可用, 挂起依赖协作式 GC, 不依赖信号上下文).
 */
#ifndef _VITA_SHIM_SIGNAL_H
#define _VITA_SHIM_SIGNAL_H

#include <sys/signal.h>

#ifndef SA_SIGINFO
#define SA_SIGINFO 0x2
#endif

#ifndef SA_RESTART
#define SA_RESTART 0x4
#endif

/* 单参 handler 收到强制转换的假 siginfo/上下文, 只在抢占挂起路径被引用,
 * Vita 上以协作式挂起为主, 该路径不会被实际触发 */
#ifndef sa_sigaction
#define sa_sigaction sa_handler
#endif

/* 残缺版 newlib signal.h 才有这些, sys/signal.h 没有 */
#ifndef SIG_DFL
#define SIG_DFL ((_sig_func_ptr)0)
#endif
#ifndef SIG_IGN
#define SIG_IGN ((_sig_func_ptr)1)
#endif
#ifndef SIG_ERR
#define SIG_ERR ((_sig_func_ptr)-1)
#endif

#ifndef SIGPWR
#define SIGPWR 27
#endif

/* raise 在部分 TU 中未包含任何声明它的头 */
int raise (int);

/* sys/signal.h 不声明 signal() (在残缺版顶层 signal.h 里) */
_sig_func_ptr signal (int signo, _sig_func_ptr func);

#endif /* _VITA_SHIM_SIGNAL_H */
