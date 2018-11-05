#!/bin/sh

VERSION=v$(date +%Y%m%d)
SUFFIX=+snapshot

if git rev-parse --is-inside-work-tree >/dev/null 2>&1 ; then
  SUFFIX=+$(git rev-parse --short HEAD)
fi

echo "${VERSION}${SUFFIX}"
