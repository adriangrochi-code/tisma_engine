#!/bin/sh
# A SIGTERM ends tisma as its window's close button does (UI21): it exits
# with 0 and writes the interface preferences.
# usage: ui_sigterm.sh HOME_DIR XVFB_RUN TISMA
home=$1
xvfb=$2
app=$3
rm -rf "$home" && mkdir -p "$home" || exit 1
HOME="$home" XDG_CONFIG_HOME= "$xvfb" -a -s "-screen 0 1280x800x24" \
  sh -c '"$0" --size 1000x700 & p=$!; sleep 6; kill -TERM $p; wait $p' "$app"
status=$?
echo "exit $status"
[ "$status" -eq 0 ] && [ -f "$home/.config/tisma/interface.json" ]
