#!/bin/sh

export CC=clang40
export CFLAGS="-Os -g -pipe -Wall -Werror -fvisibility=hidden \
     -ffunction-sections -fdata-sections"
export LDFLAGS=" -Wl,--gc-sections"
# -Wl,--build-id=none
make
