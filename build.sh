#!/bin/sh

export CFLAGS="-Os -pipe -Wall -Werror -fvisibility=hidden \
     -ffunction-sections -fdata-sections"
export LDFLAGS=" -Wl,--gc-sections"
# -Wl,--build-id=none
make -f yans.mk
