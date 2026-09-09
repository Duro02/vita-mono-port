/*
 * Vita dlfcn shim - 存根实现.
 * Vita 用户态没有动态加载; mono 的嵌入部署走 mono_dl_fallback_register.
 */
#ifndef _VITA_SHIM_DLFCN_H
#define _VITA_SHIM_DLFCN_H

#define RTLD_LAZY    0x0001
#define RTLD_NOW     0x0002
#define RTLD_GLOBAL  0x0100
#define RTLD_LOCAL   0x0000
#define RTLD_DEFAULT ((void *) 0)
#define RTLD_NEXT    ((void *) -1)

void *dlopen (const char *filename, int flag);
void *dlsym (void *handle, const char *symbol);
int dlclose (void *handle);
char *dlerror (void);

#endif /* _VITA_SHIM_DLFCN_H */
