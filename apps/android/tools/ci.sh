#!/usr/bin/env bash
# The same steps as .github/workflows/android.yml, for the work session:
# the brand icons, the tests of the cloud client and of the app, the debug APKs and the testers' (beta, R8).
# Needs a JDK (17 or newer) and the Android SDK (ANDROID_HOME or local.properties).
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/brand_icons.py --check
# Java APIs Android only has from API 33 on (the app runs from API 28): they
# compile against the JDK and fail on older phones (QA AND3).
if grep -rnE '\.(readNBytes|readAllBytes|transferTo)\(|InputStream\.nullInputStream|OutputStream\.nullOutputStream' \
     --include=*.kt cloud/src/main printers/src/main app/src/main; then
  echo "error: Java API missing on Android before API 33 (see above); use the helpers in cloud/Http.kt"
  exit 1
fi
python3 ../../tools/check_licenses.py ../..
./gradlew --no-parallel :cloud:test :printers:test :app:testDebugUnitTest :app:lintDebug :app:assembleDebug :app:assembleBeta
echo "Android checks passed: app/build/outputs/apk/debug/app-{arm64-v8a,x86_64,universal}-debug.apk"
