#!/bin/sh -e
#
# build.sh - build script aimed for release builds
#

[ -z "$NCPUS" ] && NCPUS=2

export CC=clang40
export CFLAGS="-Os -pipe -fvisibility=hidden \
-ffunction-sections -fdata-sections -fomit-frame-pointer \
-DNDEBUG -fstack-protector-strong"
export LDFLAGS="-Wl,--gc-sections"
# -Wl,--build-id=none
make -j${NCPUS}
