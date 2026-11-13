#!/usr/bin/env bash
# Small phones and large models (mobile_app.md, rules 12 and 19; QA, 13-11-2026), on a running emulator or phone
# with the debug APK installed. For each model it slices with the engine inside the app (as the app's user, with
# run-as), polls its peak memory (VmHWM) and time, and sets that against the app's own estimate
# (MemoryEstimate: 128 MB + 300 bytes a triangle) and the memory the phone has free: the app warns and offers the
# cloud when the estimate is above the free memory. A run killed for memory without that warning is a failure.
#
#   memory_check.sh <config.json> <model.stl>...      (prints one line per model; exit 1 on a missed warning)
set -u
PKG=net.tismaservidores.tisma
cfg=$1; shift
adb shell mkdir -p /data/local/tmp/mem
adb push -q "$cfg" /data/local/tmp/mem/config.json > /dev/null
adb shell chmod 644 /data/local/tmp/mem/config.json
lib=$(adb shell pm dump "$PKG" | sed -n 's/.*nativeLibraryDir=\(.*\)$/\1/p' | head -1 | tr -d '\r')
total=$(adb shell grep MemTotal /proc/meminfo | awk '{print int($2/1024)}')
missed=0
for model in "$@"; do
    name=$(basename "$model")
    adb push -q "$model" "/data/local/tmp/mem/$name" > /dev/null
    adb shell chmod 644 "/data/local/tmp/mem/$name"
    # Binary STL: the triangle count after the 80-byte header.
    tris=$(python3 -c "import struct,sys; f=open(sys.argv[1],'rb'); f.seek(80); print(struct.unpack('<I', f.read(4))[0])" "$model")
    need=$(( (128 * 1024 * 1024 + tris * 300) / 1024 / 1024 ))
    free=$(adb shell grep MemAvailable /proc/meminfo | awk '{print int($2/1024)}')
    warns=$([ "$need" -gt "$free" ] && echo yes || echo no)
    out=$(adb shell "run-as $PKG sh -c '
        start=\$(date +%s%N)
        $lib/libtisma_engine.so slice /data/local/tmp/mem/$name --config /data/local/tmp/mem/config.json \
            -o cache/mem.gcode --summary cache/mem.json --no-gpu > /dev/null 2>&1 &
        pid=\$!; peak=0
        while kill -0 \$pid 2>/dev/null; do
            h=\$(sed -n \"s/^VmHWM: *\\([0-9]*\\).*/\\1/p\" /proc/\$pid/status 2>/dev/null)
            [ -n \"\$h\" ] && peak=\$h
            sleep 0.1
        done
        wait \$pid; code=\$?
        end=\$(date +%s%N)
        rm -f cache/mem.gcode cache/mem.json
        echo \$code \$((peak / 1024)) \$(((end - start) / 1000000))
    '" | tr -d '\r')
    read -r code peak ms <<< "$out"
    verdict=ok
    if [ "$code" != 0 ] && [ "$warns" = no ]; then verdict="KILLED WITHOUT WARNING"; missed=1; fi
    if [ "$code" != 0 ] && [ "$warns" = yes ]; then verdict="warned (and it would not fit)"; fi
    if [ "$code" = 0 ] && [ "$warns" = yes ]; then verdict="warned (it would have fit)"; fi
    echo "RAM ${total} MB | $name: ${tris} triangles | free ${free} MB | estimate ${need} MB | warning ${warns} | engine exit ${code}, peak ${peak} MB, ${ms} ms | ${verdict}"
    adb shell rm -f "/data/local/tmp/mem/$name"
done
exit $missed
