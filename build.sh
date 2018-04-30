#!/bin/sh -e
#
# build.sh - build script invoking make with tuned compilation flags
#

CFLAGS_release="-Os -pipe -fvisibility=hidden \
-ffunction-sections -fdata-sections -fomit-frame-pointer \
-DNDEBUG -fstack-protector-strong"
LDFLAGS_release="-Wl,--gc-sections"
# -Wl,--build-id=none

CFLAGS_dev="-O0 -g -pipe -fsanitize=address -fstack-protector-strong"
LDFLAGS_dev=""

die() {
  printf '%s\n' "$1" >&2
  exit 1
}

[ -z "$NCPUS" ] && NCPUS=2
[ "$1" != release -a "$1" != dev ] && die "usage: $0 <release|dev>"
BUILDTYPE=$1
LASTBUILD=$(cat .lastbuild 2>/dev/null || exit 0)

export CC=$(command -v clang40 || command -v clang || command -v gcc)

# TODO: substitution instead of conditional, as with makefiles?
if [ "$BUILDTYPE" = release ]; then
  export CFLAGS=$CFLAGS_release
  export LDFLAGS=$LDFLAGS_release
elif [ "$BUILDTYPE" = dev ]; then
  export CFLAGS=$CFLAGS_dev
  export LDFLAGS=$LDFLAGS_dev
  export DATAROOTDIR=$(pwd)/data
  export LOCALSTATEDIR=$(pwd)/lel
else
  die "invalid build type"
fi

if [ "$LASTBUILD" != "$BUILDTYPE" ]; then
  echo "$BUILDTYPE" > .lastbuild
  make -j${NCPUS} distclean
fi

make -j${NCPUS} all
make -j${NCPUS} check
