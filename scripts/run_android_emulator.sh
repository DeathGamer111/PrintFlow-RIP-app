#!/bin/bash

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build-android}"
AVD_NAME="${AVD_NAME:-RIP_App_Test}"
PACKAGE_NAME="${PACKAGE_NAME:-com.ripapp.printer}"
ACTIVITY_NAME="${ACTIVITY_NAME:-org.qtproject.qt.android.bindings.QtActivity}"

fail() {
    printf 'error: %s\n' "$1" >&2
    exit 1
}

prepend_if_dir() {
    if [[ -d "$1" ]]; then
        PATH="$1:${PATH}"
    fi
}

if [[ -n "${ANDROID_SDK_ROOT:-}" ]]; then
    prepend_if_dir "${ANDROID_SDK_ROOT}/platform-tools"
    prepend_if_dir "${ANDROID_SDK_ROOT}/emulator"
fi

command -v adb >/dev/null 2>&1 || fail "Missing command: adb"
command -v emulator >/dev/null 2>&1 || fail "Missing command: emulator"

APK_PATH="${APK_PATH:-}"
if [[ -z "${APK_PATH}" ]]; then
    APK_PATH="$(find "${BUILD_DIR}/android-build/build/outputs/apk" -type f -name '*.apk' 2>/dev/null | sort | tail -n 1 || true)"
fi

[[ -n "${APK_PATH}" && -f "${APK_PATH}" ]] || fail "APK not found. Run scripts/dev_build_android.sh first or set APK_PATH."

if ! adb get-state >/dev/null 2>&1; then
    printf 'Starting emulator: %s\n' "${AVD_NAME}"
    emulator -avd "${AVD_NAME}" -netdelay none -netspeed full >/tmp/rip-app-emulator.log 2>&1 &
fi

adb wait-for-device
adb shell getprop sys.boot_completed | grep -q 1 || {
    printf 'Waiting for Android boot to complete...\n'
    until adb shell getprop sys.boot_completed | grep -q 1; do
        sleep 2
    done
}

adb install -r "${APK_PATH}"
adb shell am start -n "${PACKAGE_NAME}/${ACTIVITY_NAME}"

printf '\nApp launched. Streaming filtered logcat; press Ctrl+C to stop.\n'
adb logcat | grep --line-buffered -E 'RIP|Qt|libSYPrintAPIforPROII|Nocai|AndroidRuntime'
