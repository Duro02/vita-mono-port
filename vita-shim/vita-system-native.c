/*
 * Vita System.Native 适配层.
 * mscorlib 的 Unix 文件 IO (Interop.Sys) 经 pinvoke 调用 System.Native;
 * Vita 上没有该库, 这里用 newlib/sce 实现并经 mono_dl_fallback_register 注册.
 * PAL 错误码即 errno (coreclr Unix PAL 与 errno 一致), 直接透传.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <mono/utils/mono-dl-fallback.h>

/* 与 mscorlib 中 Interop/Sys/FileStatus 逐字段对应 (sequential, 4 字节对齐) */
struct VitaFileStatus {
	int           Flags;
	int           Mode;
	unsigned int  Uid;
	unsigned int  Gid;
	long long     Size;
	long long     ATime;
	long long     ATimeNsec;
	long long     MTime;
	long long     MTimeNsec;
	long long     CTime;
	long long     CTimeNsec;
	long long     BirthTime;
	long long     BirthTimeNsec;
	long long     Dev;
	long long     Ino;
	unsigned int  UserFlags;
};

static void
fill_status (struct VitaFileStatus *out, struct stat *st)
{
	memset (out, 0, sizeof (*out));
	out->Mode = (int) st->st_mode;
	out->Uid  = (unsigned int) st->st_uid;
	out->Gid  = (unsigned int) st->st_gid;
	out->Size = (long long) st->st_size;
	out->ATime = (long long) st->st_atime;
	out->MTime = (long long) st->st_mtime;
	out->CTime = (long long) st->st_ctime;
	out->Dev  = (long long) st->st_dev;
	out->Ino  = (long long) st->st_ino;
}

int
SystemNative_ConvertErrorPlatformToPal (int err)
{
	return err;
}

int
SystemNative_ConvertErrorPalToPlatform (int err)
{
	return err;
}

unsigned char *
SystemNative_StrErrorR (int err, unsigned char *buf, int bufsize)
{
	/* newlib 头声明为 POSIX 语义 (返回 int, 消息写入 buf) */
	strerror_r (err, (char *) buf, (size_t) bufsize);
	return buf;
}

void
SystemNative_GetNonCryptographicallySecureRandomBytes (unsigned char *buffer, int length)
{
	int i;
	/* 简单 xorshift, 非密码学用途 (GetNonCryptographicallySecure) */
	static unsigned int s = 0;
	if (!s)
		s = (unsigned int) ((uintptr_t) buffer ^ 0x9E3779B9u);
	for (i = 0; i < length; i++) {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		buffer [i] = (unsigned char) (s & 0xFF);
	}
}

int
SystemNative_GetReadDirRBufferSize (void)
{
	return 1024;
}

int
SystemNative_ReadLink (const char *path, unsigned char *buffer, int bufferSize)
{
	(void)path; (void)buffer; (void)bufferSize;
	errno = ENOSYS;
	return -1;
}

static int
do_fstat (int fd, struct VitaFileStatus *out)
{
	struct stat st;
	if (fstat (fd, &st) < 0)
		return -1;
	fill_status (out, &st);
	return 0;
}

static int
do_stat (const char *path, struct VitaFileStatus *out, int lstat)
{
	struct stat st;
	int r = stat (path, &st); /* Vita 无 symlink, lstat==stat */
	(void) lstat;
	if (r < 0)
		return -1;
	fill_status (out, &st);
	return 0;
}

int
SystemNative_FStat2 (int fd, struct VitaFileStatus *out)
{
	return do_fstat (fd, out);
}

int
SystemNative_Stat2 (const char *path, struct VitaFileStatus *out)
{
	return do_stat (path, out, 0);
}

int
SystemNative_LStat2 (const char *path, struct VitaFileStatus *out)
{
	return do_stat (path, out, 1);
}

int
SystemNative_Symlink (const char *target, const char *linkpath)
{
	(void)target; (void)linkpath;
	errno = ENOSYS;
	return -1;
}

int
SystemNative_ChMod (const char *path, int mode)
{
	return chmod (path, (mode_t) mode);
}

int
SystemNative_CopyFile (int srcFd, int dstFd)
{
	char buf [32768];
	ssize_t r;
	if (lseek (srcFd, 0, SEEK_SET) < 0)
		return -1;
	if (lseek (dstFd, 0, SEEK_SET) < 0)
		return -1;
	for (;;) {
		r = read (srcFd, buf, sizeof (buf));
		if (r < 0)
			return -1;
		if (r == 0)
			break;
		if (write (dstFd, buf, (size_t) r) != r)
			return -1;
	}
	return 0;
}

unsigned int
SystemNative_GetEGid (void)
{
	return 0;
}

unsigned int
SystemNative_GetEUid (void)
{
	return 0;
}

int
SystemNative_LChflags (const char *path, unsigned int flags)
{
	(void)path; (void)flags;
	errno = ENOSYS;
	return -1;
}

int
SystemNative_LChflagsCanSetHiddenFlag (void)
{
	return 0;
}

int
SystemNative_Link (const char *src, const char *dst)
{
	(void)src; (void)dst;
	errno = ENOSYS;
	return -1;
}

int
SystemNative_MkDir (const char *path, int mode)
{
	return mkdir (path, (mode_t) mode);
}

int
SystemNative_Rename (const char *oldpath, const char *newpath)
{
	return rename (oldpath, newpath);
}

int
SystemNative_RmDir (const char *path)
{
	return rmdir (path);
}

int
SystemNative_UTime (const char *path, long long seconds)
{
	struct utimbuf t;
	t.actime = (long) seconds;
	t.modtime = (long) seconds;
	return utime (path, &t);
}

int
SystemNative_UTimes (const char *path, long long *times)
{
	/* times = [atime_sec, atime_usec, mtime_sec, mtime_usec] */
	struct utimbuf t;
	t.actime = (long) times [0];
	t.modtime = (long) times [2];
	return utime (path, &t);
}

int
SystemNative_Unlink (const char *path)
{
	return unlink (path);
}

void *
SystemNative_OpenDir (const char *path)
{
	return opendir (path);
}

int
SystemNative_ReadDirR (void *dir, unsigned char *buffer, int bufferSize, void **result)
{
	struct dirent *e = readdir ((DIR *) dir);
	if (!e) {
		*result = NULL;
		return 0;
	}
	{
		size_t n = strlen (e->d_name);
		if ((int) (n + 1) > bufferSize)
			return -1;
		memcpy (buffer, e->d_name, n + 1);
	}
	*result = dir;
	return 0;
}

int
SystemNative_CloseDir (void *dir)
{
	return closedir ((DIR *) dir);
}

/* ---- 符号表 ---- */

struct VitaNativeSym {
	const char *name;
	void       *func;
};

static struct VitaNativeSym vita_native_syms [] = {
	{ "SystemNative_ConvertErrorPlatformToPal", SystemNative_ConvertErrorPlatformToPal },
	{ "SystemNative_ConvertErrorPalToPlatform", SystemNative_ConvertErrorPalToPlatform },
	{ "SystemNative_StrErrorR", SystemNative_StrErrorR },
	{ "SystemNative_GetNonCryptographicallySecureRandomBytes", SystemNative_GetNonCryptographicallySecureRandomBytes },
	{ "SystemNative_GetReadDirRBufferSize", SystemNative_GetReadDirRBufferSize },
	{ "SystemNative_ReadLink", SystemNative_ReadLink },
	{ "SystemNative_FStat2", SystemNative_FStat2 },
	{ "SystemNative_Stat2", SystemNative_Stat2 },
	{ "SystemNative_LStat2", SystemNative_LStat2 },
	{ "SystemNative_Symlink", SystemNative_Symlink },
	{ "SystemNative_ChMod", SystemNative_ChMod },
	{ "SystemNative_CopyFile", SystemNative_CopyFile },
	{ "SystemNative_GetEGid", SystemNative_GetEGid },
	{ "SystemNative_GetEUid", SystemNative_GetEUid },
	{ "SystemNative_LChflags", SystemNative_LChflags },
	{ "SystemNative_LChflagsCanSetHiddenFlag", SystemNative_LChflagsCanSetHiddenFlag },
	{ "SystemNative_Link", SystemNative_Link },
	{ "SystemNative_MkDir", SystemNative_MkDir },
	{ "SystemNative_Rename", SystemNative_Rename },
	{ "SystemNative_RmDir", SystemNative_RmDir },
	{ "SystemNative_UTime", SystemNative_UTime },
	{ "SystemNative_UTimes", SystemNative_UTimes },
	{ "SystemNative_Unlink", SystemNative_Unlink },
	{ "SystemNative_OpenDir", SystemNative_OpenDir },
	{ "SystemNative_ReadDirR", SystemNative_ReadDirR },
	{ "SystemNative_CloseDir", SystemNative_CloseDir },
	{ "snprintf", snprintf },
	{ NULL, NULL }
};

static void *
vita_dl_load (const char *name, int flags, char **err, void *user_data)
{
	size_t i;
	(void)flags; (void)user_data;
	/* System.Native 与 libc 由本表提供 */
	if (!strcmp (name, "System.Native") || !strcmp (name, "libc") ||
	    !strcmp (name, "System.Native.dll") || !strcmp (name, "libc.dll"))
		return (void *) vita_native_syms;
	if (err)
		*err = NULL;
	return NULL;
}

static void *
vita_dl_symbol (void *handle, const char *name, char **err, void *user_data)
{
	size_t i;
	(void)user_data;
	if (handle != (void *) vita_native_syms) {
		if (err)
			*err = NULL;
		return NULL;
	}
	for (i = 0; vita_native_syms [i].name; i++) {
		if (!strcmp (vita_native_syms [i].name, name))
			return vita_native_syms [i].func;
	}
	if (err)
		*err = NULL;
	return NULL;
}

static void *
vita_dl_close (void *handle, void *user_data)
{
	(void)handle; (void)user_data;
	return NULL;
}

void vita_register_dllmap (void);
void
vita_register_dllmap (void)
{
	mono_dl_fallback_register (vita_dl_load, vita_dl_symbol, vita_dl_close, NULL);
}
