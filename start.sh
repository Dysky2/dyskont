#!/bin/bash

SESJA="symulacja"
CMD="make; ./dyskont"

tmux has-session -t "$SEJSA" 2>/dev/null

if [ $? != 0 ]; then

    tmux new-session -d -s "$SESJA"
    tmux send-keys -t "$SESJA" "$CMD" C-m

fi

tmux attach -t "$SESJA"