#!/bin/bash
# Mono 6.12.0.99 -> PS Vita (arm-vita-eabi) 交叉构建脚本
# 用法: ./build-mono-vita.sh [configure|make|all]
set -e

MONO_SRC=${MONO_SRC:-$HOME/projects/mono}
PREFIX=${PREFIX:-$HOME/projects/mono-vita-install}

# 交叉编译环境修正（详见 patches/ 与 README 决策记录）:
# -DSTRERROR_R_CHAR_P : newlib 的 strerror_r 是 GNU 语义(char*)，
#                       AC_FUNC_STRERROR_R 交叉时探测不了，强制指定
# -DHAVE_MREMAP=0     : Vita 内核无 mremap；newlib 定义 __NetBSD__ 导致 dlmalloc 误开
# 补上缺失 POSIX 原语的 shim (mmap/sched_yield/存根, 见 vita-shim/)
SHIM=${SHIM:-$HOME/projects/vita-mono-port/vita-shim}
# -D_POSIX_C_SOURCE/-D_DEFAULT_SOURCE:
#   newlib 的完整 sigaction 只在 rtems 分支, 用 shim 兼容;
#   SIGPWR 等需 _DEFAULT_SOURCE 才可见 (注意 __BSD_VISIBLE 会被 features.h 覆盖)
export CPPFLAGS="-DSTRERROR_R_CHAR_P -DHAVE_MREMAP=0 -D_POSIX_VERSION=200112L -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DSSIZE_MAX=2147483647 -DPATH_MAX=4096 -include strings.h -include signal.h -I$SHIM/include"

# 工具链: configure 不会自己找 vitasdk 的编译器, 必须显式指定
export PATH=/usr/local/vitasdk/bin:$PATH
export CC=arm-vita-eabi-gcc
export CXX=arm-vita-eabi-g++
export AR=arm-vita-eabi-ar
export AS=arm-vita-eabi-as
export LD=arm-vita-eabi-ld
export RANLIB=arm-vita-eabi-ranlib
export STRIP=arm-vita-eabi-strip
export OBJDUMP=arm-vita-eabi-objdump

CONF_FLAGS="--host=arm-vita-eabi \
  --prefix=$PREFIX \
  --disable-mcs-build \
  --disable-boehm \
  --with-sgen=yes \
  --with-xen=no \
  --with-x=no \
  --disable-nls \
  --with-ikvm=no \
  --disable-shared \
  --enable-static \
  ac_cv_struct_tm_gmtoff=yes"

# -DHAVE_MMAP=1 : configure 检测不到 libc 里的 mmap (在 vita-shim 里), 强制启用
#                 mono-mmap.c 的 mono_mmap/mono_file_map 全靠它
export CPPFLAGS="$CPPFLAGS -DHAVE_MMAP=1"

MAKE_TARGET=${MAKE_TARGET:-libmonosgen-2.0.la}

cd "$MONO_SRC"

case "${1:-all}" in
  configure)
    ./configure $CONF_FLAGS
    ;;
  make)
    make -C mono/mini -j$(nproc) $MAKE_TARGET
    ;;
  all)
    ./configure $CONF_FLAGS
    make -C mono/mini -j$(nproc) $MAKE_TARGET
    ;;
esac
