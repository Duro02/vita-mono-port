#!/bin/bash
# M2: 构建 Vita Mono 启动器 (链接 libmonosgen + vita-shim)
set -e

export PATH=/usr/local/vitasdk/bin:$PATH
VITASDK=/usr/local/vitasdk
ROOT=$HOME/Projects/vita-port/vita-mono-port
MONO_SRC=${MONO_SRC:-$HOME/Projects/vita-port/mono}
OUT=$ROOT/vita-launcher/build
mkdir -p "$OUT"

# 1. vita-shim 静态库
MONO_SRC_INC="-I$MONO_SRC -I$MONO_SRC/mono -I$MONO_SRC/mono/eglib"
arm-vita-eabi-gcc -c "$ROOT/vita-shim/vita-shim.c" \
	-I"$ROOT/vita-shim/include" -O2 -o "$OUT/vita-shim.o"
arm-vita-eabi-gcc -c "$ROOT/vita-shim/vita-system-native.c" \
	-I"$ROOT/vita-shim/include" $MONO_SRC_INC -O2 -o "$OUT/vita-system-native.o"
arm-vita-eabi-ar rcs "$OUT/libvitashim.a" "$OUT/vita-shim.o" "$OUT/vita-system-native.o"

# 2. C# hello world (host mcs, net_4_x profile)
mkdir -p "$OUT/bcl"
mcs -out:"$OUT/bcl/hello.exe" "$ROOT/hello/hello.cs"
cp /usr/lib/mono/4.5/mscorlib.dll "$OUT/bcl/"

# 3. 链接启动器
MONO_INC="-I$MONO_SRC -I$MONO_SRC/mono -I$MONO_SRC/mono/eglib -I$MONO_SRC/mono/utils -I$MONO_SRC/mono/metadata -I$MONO_SRC/mono/mini -I$MONO_SRC/mono/sgen"

arm-vita-eabi-gcc "$ROOT/vita-launcher/main.c" -o "$OUT/launcher.elf" \
	$MONO_INC \
	-D_STRERROR_R_CHAR_P -DHAVE_CONFIG_H \
	-O2 -Wl,-q -Wl,--wrap=sysconf,--wrap=open,--wrap=close,--wrap=stat,--wrap=fstat,--wrap=mkdir,--wrap=rmdir,--wrap=unlink,--wrap=rename,--wrap=opendir,--wrap=readdir,--wrap=closedir,--wrap=rewinddir,--wrap=pthread_create,--wrap=write,--wrap=abort,--wrap=raise,--wrap=fcntl,--wrap=getrusage,--wrap=pthread_kill,--wrap=pthread_cond_wait,--wrap=pthread_cond_timedwait \
	"$MONO_SRC/mono/mini/.libs/libmonosgen-2.0.a" \
	"$MONO_SRC/mono/metadata/.libs/libmonoruntimesgen.a" \
	"$MONO_SRC/mono/metadata/.libs/libmonoruntime-config.a" \
	"$MONO_SRC/mono/metadata/.libs/libmonoruntime-support.a" \
	"$MONO_SRC/mono/sgen/.libs/libmonosgen.a" \
	"$MONO_SRC/mono/utils/.libs/libmonoutils.a" \
	"$MONO_SRC/mono/utils/.libs/libmonomath.a" \
	"$MONO_SRC/mono/eglib/.libs/libeglib.a" \
	"$OUT/libvitashim.a" \
	-lpthread -lz -lvita2d -lm \
	-lScePgf_stub -lScePvf_stub -lSceDisplay_stub -lSceGxm_stub \
	-lSceSysmodule_stub -lSceAppMgr_stub -lSceProcessmgr_stub \
	-lSceCommonDialog_stub -lSceCtrl_stub -lSceTouch_stub \
	-lSceRtc_stub -lSceKernelThreadMgr_stub -lSceKernelModulemgr_stub \
	-lSceNet_stub -lSceNetCtl_stub -lSceSysmem_stub -lSceLibKernel_stub

# 4. VPK
cd "$OUT"
vita-elf-create launcher.elf launcher.velf
vita-make-fself -s -c launcher.velf eboot.bin
vita-mksfoex -s TITLE_ID=MONO00002 -s APP_VER=01.00 "Mono Vita Test" param.sfo
mkdir -p vpkdir/sce_sys
cp param.sfo vpkdir/sce_sys/
cp eboot.bin vpkdir/eboot.bin
vita-pack-vpk -s param.sfo -b eboot.bin MONO00002.vpk
echo "BUILD OK: $OUT/MONO00002.vpk"
