#!/bin/sh
mkdir lel
tmux new-session -s "yansd" -d ./apps/ethd/ethd -n -b "$(pwd)/lel"
tmux split-window -v -p 66 ./apps/clid/clid -n -b "$(pwd)/lel"
tmux split-window -v -p 50 ./apps/stored/stored -n -b "$(pwd)/lel"
tmux -2 attach-session -d

