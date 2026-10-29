#!/bin/sh
# A SIGTERM ends tisma as its window's close button does (UI21): it exits
# with 0 and writes the interface preferences.
# With a DELAY under a second, the signal comes while the window opens.
# usage: ui_sigterm.sh HOME_DIR XVFB_RUN TISMA [DELAY]
home=$1
xvfb=$2
app=$3
delay=${4:-6}
rm -rf "$home" && mkdir -p "$home" || exit 1
HOME="$home" XDG_CONFIG_HOME= "$xvfb" -a -s "-screen 0 1280x800x24" \
  sh -c '"$0" --size 1000x700 & p=$!; sleep "$1"; kill -TERM $p; wait $p' "$app" "$delay"
status=$?
echo "exit $status"
[ "$status" -eq 0 ] && [ -f "$home/.config/tisma/interface.json" ]
