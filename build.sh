#!/bin/sh

export CC=gcc
export CFLAGS="-Os -g -pipe -Wall -Werror -fvisibility=hidden \
-ffunction-sections -fdata-sections -fomit-frame-pointer \
-DNDEBUG -fstack-protector-strong"
export LDFLAGS=" -Wl,--gc-sections"
# -Wl,--build-id=none
make -j2
