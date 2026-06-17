#!/bin/bash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${REPO_ROOT}/.android-sdk}"
AVD_NAME="${AVD_NAME:-${ANDROID_AVD_NAME:-PrintFlow_Pixel}}"
ANDROID_ABI="${ANDROID_ABI:-x86_64}"
BUILD_DIR="${BUILD_DIR:-build-android}"

export ANDROID_SDK_ROOT
export ANDROID_ABI
export BUILD_DIR

prepend_if_dir() {
    if [[ -d "$1" ]]; then
        PATH="$1:${PATH}"
    fi
}

prepend_if_dir "${ANDROID_SDK_ROOT}/platform-tools"
prepend_if_dir "${ANDROID_SDK_ROOT}/emulator"
prepend_if_dir "${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin"
export PATH

scripts/dev_build_android.sh

AVD_NAME="${AVD_NAME}" BUILD_DIR="${BUILD_DIR}" scripts/run_android_emulator.sh
