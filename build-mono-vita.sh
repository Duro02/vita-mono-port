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
VITA_CPPFLAGS="-DSTRERROR_R_CHAR_P -DHAVE_MREMAP=0"

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
  CPPFLAGS=\"$VITA_CPPFLAGS\""

cd "$MONO_SRC"

case "${1:-all}" in
  configure)
    ./configure $CONF_FLAGS
    ;;
  make)
    make -j$(nproc)
    ;;
  all)
    ./configure $CONF_FLAGS
    make -j$(nproc)
    ;;
esac
