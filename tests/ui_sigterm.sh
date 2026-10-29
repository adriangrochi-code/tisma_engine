#!/bin/sh
# A SIGTERM ends tisma as its window's close button does (UI21): it exits
# with 0 and writes the interface preferences.
# With "start" for DELAY, tisma sends the signal to itself before its window
# opens (--term-while-starting): a sleep that short would race the loader.
# usage: ui_sigterm.sh HOME_DIR XVFB_RUN TISMA [DELAY]
home=$1
xvfb=$2
app=$3
delay=${4:-6}
rm -rf "$home" && mkdir -p "$home" || exit 1
if [ "$delay" = start ]; then
  HOME="$home" XDG_CONFIG_HOME= "$xvfb" -a -s "-screen 0 1280x800x24" "$app" --size 1000x700 --term-while-starting
else
  HOME="$home" XDG_CONFIG_HOME= "$xvfb" -a -s "-screen 0 1280x800x24" \
    sh -c '"$0" --size 1000x700 & p=$!; sleep "$1"; kill -TERM $p; wait $p' "$app" "$delay"
fi
status=$?
echo "exit $status"
[ "$status" -eq 0 ] && [ -f "$home/.config/tisma/interface.json" ]
