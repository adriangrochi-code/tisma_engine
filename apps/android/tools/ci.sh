#!/usr/bin/env bash
# The same steps as .github/workflows/android.yml, for the work session:
# the brand icons, the tests of the cloud client and of the app, and a debug APK.
# Needs a JDK (17 or newer) and the Android SDK (ANDROID_HOME or local.properties).
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/brand_icons.py --check
python3 ../../tools/check_licenses.py ../..
./gradlew --no-parallel :cloud:test :app:testDebugUnitTest :app:assembleDebug
echo "Android checks passed: app/build/outputs/apk/debug/app-debug.apk"
