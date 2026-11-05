#!/bin/sh
# xvfb-run for the interface's tests, without the race of its -a: two tests
# started at once could take the same display and one failed in 0.3 s with
# «Failed to open display» (the full test runs of 2026-11-05). Each run takes
# a display number from its process id instead, and a run that still cannot
# open its display is tried again, at most three times. Takes xvfb-run's own
# arguments; -a is dropped. Own design.
real=$(command -v xvfb-run) || { echo "xvfb-run not found" >&2; exit 1; }
first=1
for a in "$@"; do
  if [ "$first" = 1 ]; then set --; first=0; fi
  [ "$a" = "-a" ] || set -- "$@" "$a"
done
out=$(mktemp) || exit 1
try=1
while :; do
  n=$(( ( ($$ * 7 + try * 131) % 900 ) + 100 ))
  "$real" -n "$n" "$@" >"$out" 2>&1
  code=$?
  if [ "$try" -lt 3 ] && grep -q -e "Failed to open display" -e "Server is already active" "$out"; then
    try=$((try + 1))
    sleep 1
    continue
  fi
  cat "$out"
  rm -f "$out"
  exit "$code"
done
