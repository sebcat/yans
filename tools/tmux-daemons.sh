#!/bin/sh

die() {
  printf '%s\n' "$1" >&2
  exit 1
}

if [ ! -x ./apps/ethd/ethd -o \
     ! -x ./apps/clid/clid -o \
     ! -x ./apps/stored/stored ]; then
  die "one or more daemons missing"
fi

if [ ! -d ./lel ]; then
  mkdir ./lel || die "failed to create ./lel"
fi

tmux new-session -s "yansd" -d ./apps/ethd/ethd -n -b "$(pwd)/lel"
tmux split-window -v -p 66 ./apps/clid/clid -n -b "$(pwd)/lel"
tmux split-window -v -p 50 ./apps/stored/stored -n -b "$(pwd)/lel"
tmux -2 attach-session -d

