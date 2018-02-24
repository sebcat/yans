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

MAYBE_VALGRIND=
if [ "$USE_VALGRIND" = 1 ]; then
  MAYBE_VALGRIND="valgrind --error-exitcode=1 --leak-check=full \
--log-file=run.%p --trace-children=yes"
fi

tmux new-session -s "yansd" -d $MAYBE_VALGRIND \
    ./apps/ethd/ethd -n -b "$(pwd)/lel"
# clid uses syscalls not implemented in some versions of valgrind
tmux split-window -v -p 66 ./apps/clid/clid -n -b "$(pwd)/lel"
tmux split-window -v -p 50 $MAYBE_VALGRIND \
    ./apps/stored/stored -n -b "$(pwd)/lel"
tmux -2 attach-session -d

