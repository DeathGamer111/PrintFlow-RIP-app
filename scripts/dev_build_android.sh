#!/bin/bash

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build-android}"
ANDROID_ABI="${ANDROID_ABI:-x86_64}"

fail() {
    printf 'error: %s\n' "$1" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "Missing command: $1"
}

require_path() {
    [[ -e "$2" ]] || fail "$1 does not exist: $2"
}

prepend_if_dir() {
    if [[ -d "$1" ]]; then
        PATH="$1:${PATH}"
    fi
}

[[ -n "${QT_ANDROID_CMAKE:-}" ]] || fail "Set QT_ANDROID_CMAKE to the Qt Android qt-cmake path, for example ~/Qt/6.x.x/android_arm64_v8a/bin/qt-cmake"
require_path "QT_ANDROID_CMAKE" "${QT_ANDROID_CMAKE}"
[[ -n "${ANDROID_SDK_ROOT:-}" ]] || fail "Set ANDROID_SDK_ROOT to your Android SDK path"
require_path "ANDROID_SDK_ROOT" "${ANDROID_SDK_ROOT}"
[[ -n "${ANDROID_NDK_ROOT:-}" ]] || fail "Set ANDROID_NDK_ROOT to your Android NDK path"
require_path "ANDROID_NDK_ROOT" "${ANDROID_NDK_ROOT}"

prepend_if_dir "${ANDROID_SDK_ROOT}/platform-tools"
prepend_if_dir "${ANDROID_SDK_ROOT}/emulator"
prepend_if_dir "${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin"
prepend_if_dir "${ANDROID_NDK_ROOT}/prebuilt/linux-x86_64/bin"
if [[ -z "${JAVA_HOME:-}" && -x "${ANDROID_SDK_ROOT}/jdk/bin/java" ]]; then
    export JAVA_HOME="${ANDROID_SDK_ROOT}/jdk"
fi
prepend_if_dir "${JAVA_HOME:-}/bin"

require_command java
require_command ninja
require_command adb
require_command emulator

if [[ -n "${DIRECT_PRINT_SDK_ROOT:-}" && "${ANDROID_ABI}" == "arm64-v8a" ]]; then
    require_path "DIRECT_PRINT_SDK_ROOT" "${DIRECT_PRINT_SDK_ROOT}"
    if [[ ! -f "${DIRECT_PRINT_SDK_ROOT}/libSYPrintAPIforPROII.so" ]]; then
        fail "DIRECT_PRINT_SDK_ROOT must contain libSYPrintAPIforPROII.so for Android packaging"
    fi
elif [[ -n "${DIRECT_PRINT_SDK_ROOT:-}" ]]; then
    printf 'warning: DIRECT_PRINT_SDK_ROOT is set, but direct-print SDK packaging is only enabled for ANDROID_ABI=arm64-v8a. Current ABI: %s\n' "${ANDROID_ABI}" >&2
else
    printf 'warning: DIRECT_PRINT_SDK_ROOT is not set; APK will build without the direct-print SDK library.\n' >&2
fi

"${QT_ANDROID_CMAKE}" \
    -S . \
    -B "${BUILD_DIR}" \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DANDROID_SDK_ROOT="${ANDROID_SDK_ROOT}" \
    -DANDROID_NDK_ROOT="${ANDROID_NDK_ROOT}" \
    -DQT_ANDROID_ABIS="${ANDROID_ABI}"

cmake --build "${BUILD_DIR}" --target apk --parallel

printf '\nAPK build complete. Look under %s/android-build/build/outputs/apk/.\n' "${BUILD_DIR}"
