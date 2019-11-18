#!/bin/sh
# Copyright (c) 2019 Sebastian Cato
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

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
mkdir -p ./lel/clid ./lel/stored ./lel/knegd ./lel/ethd

MAYBE_VALGRIND=
if [ "$USE_VALGRIND" = 1 ]; then
  MAYBE_VALGRIND="valgrind --error-exitcode=1 --leak-check=full \
--log-file=run.%p --trace-children=yes"
fi

tmux new-session -s "yansd" -d $MAYBE_VALGRIND \
    ./apps/ethd/ethd -n
# clid uses syscalls not implemented in some versions of valgrind
tmux split-window -v -p 75 ./apps/clid/clid -n
tmux split-window -v -p 66 $MAYBE_VALGRIND \
    ./apps/stored/stored -n
tmux split-window -v -p 50 $MAYBE_VALGRIND \
    ./apps/knegd/knegd -n
tmux -2 attach-session -d

