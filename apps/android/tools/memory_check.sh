#!/usr/bin/env bash
# Small phones and large models (mobile_app.md, rules 12 and 19; QA, 13-11-2026), on a running emulator or phone
# with the debug APK installed. For each model it slices with the engine inside the app (as the app's user, with
# run-as), polls its peak memory (VmHWM) and time, and sets that against the app's own estimate
# (MemoryEstimate: 128 MB + 300 bytes a triangle) and the memory the phone has free: the app warns and offers the
# cloud when the estimate is above the free memory. A run killed for memory without that warning is a failure.
#
#   memory_check.sh <config.json> <model.stl>...      (prints one line per model; exit 1 on a missed warning)
set -u -o pipefail
PKG=net.tismaservidores.tisma
cfg=$1; shift
adb shell mkdir -p /data/local/tmp/mem
adb push -q "$cfg" /data/local/tmp/mem/config.json > /dev/null
adb shell chmod 644 /data/local/tmp/mem/config.json
# The engine is unpacked next to the installed APK: <dir of base.apk>/lib/<abi>/libtisma_engine.so.
apk=$(adb shell pm path "$PKG" | head -1 | tr -d '\r' | sed 's/^package://')
abi=$(adb shell getprop ro.product.cpu.abi | tr -d '\r')
lib="$(dirname "$apk")/lib/${abi%%-*}"
[ "$abi" = arm64-v8a ] && lib="$(dirname "$apk")/lib/arm64"
adb shell "run-as $PKG test -x $lib/libtisma_engine.so" || { echo "No engine at $lib"; exit 2; }
total=$(adb shell grep MemTotal /proc/meminfo | awk '{print int($2/1024)}')
missed=0
# The run on the phone, as a file so no quoting stands between it and the shell there: the engine in the
# background, its peak resident memory (VmHWM) polled every 0.1 s, then exit code, peak MB, ms, G-code bytes and
# layers (from the summary).
run=$(mktemp)
cat > "$run" <<'RUN'
lib=$1; model=$2
start=$(cut -d' ' -f1 /proc/uptime | tr -d .)
"$lib/libtisma_engine.so" slice "$model" --config /data/local/tmp/mem/config.json -o cache/mem.gcode \
    --summary cache/mem.json > /dev/null 2> cache/mem.err &
pid=$!; peak=0
while kill -0 $pid 2>/dev/null; do
    h=$(grep VmHWM /proc/$pid/status 2>/dev/null | tr -s ' ' | cut -d' ' -f2)
    [ -n "$h" ] && peak=$h
    sleep 0.1
done
wait $pid; code=$?
end=$(cut -d' ' -f1 /proc/uptime | tr -d .)
bytes=$(stat -c %s cache/mem.gcode 2>/dev/null || echo 0)
layers=$(grep -o '"layers": *[0-9]*' cache/mem.json 2>/dev/null | grep -o '[0-9]*$')
[ "$code" != 0 ] && tail -3 cache/mem.err
rm -f cache/mem.gcode cache/mem.json
echo RESULT $code $((peak / 1024)) $(((end - start) * 10)) $bytes ${layers:-?}
RUN
adb push -q "$run" /data/local/tmp/mem/run.sh > /dev/null
adb shell chmod 644 /data/local/tmp/mem/run.sh
for model in "$@"; do
    name=$(basename "$model")
    adb push -q "$model" "/data/local/tmp/mem/$name" > /dev/null
    adb shell chmod 644 "/data/local/tmp/mem/$name"
    # Binary STL: the triangle count after the 80-byte header.
    tris=$(python3 -c "import struct,sys; f=open(sys.argv[1],'rb'); f.seek(80); print(struct.unpack('<I', f.read(4))[0])" "$model")
    need=$(( (128 * 1024 * 1024 + tris * 300) / 1024 / 1024 ))
    free=$(adb shell grep MemAvailable /proc/meminfo | awk '{print int($2/1024)}')
    warns=$([ "$need" -gt "$free" ] && echo yes || echo no)
    out=$(adb shell "run-as $PKG sh /data/local/tmp/mem/run.sh $lib /data/local/tmp/mem/$name" | tr -d '\r')
    echo "$out" | grep -v '^RESULT' >&2
    read -r _ code peak ms bytes layers <<< "$(echo "$out" | grep "^RESULT")"
    verdict=ok
    if [ "$code" = 0 ] && [ "${bytes:-0}" -lt 1000 ]; then verdict="NO G-CODE"; missed=1; fi
    # 137: killed (SIGKILL, what the low-memory killer sends); any other failure is a fault of the check or engine.
    if [ "$code" != 0 ] && [ "$code" != 137 ]; then verdict="ENGINE FAILED (exit $code)"; missed=1; fi
    if [ "$code" = 137 ] && [ "$warns" = no ]; then verdict="KILLED WITHOUT WARNING"; missed=1; fi
    if [ "$code" = 137 ] && [ "$warns" = yes ]; then verdict="warned (and it would not fit)"; fi
    if [ "$code" = 0 ] && [ "$warns" = yes ]; then verdict="warned (it would have fit)"; fi
    echo "RAM ${total} MB | $name: ${tris} triangles | free ${free} MB | estimate ${need} MB | warning ${warns} | engine exit ${code}, peak ${peak} MB, ${ms} ms, G-code ${bytes} bytes, ${layers} layers | ${verdict}"
    adb shell rm -f "/data/local/tmp/mem/$name"
done
exit $missed
