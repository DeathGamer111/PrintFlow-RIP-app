#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mode=""
pass_args=()

usage() {
    cat <<EOF
Usage: ./Dev_Build_App.sh [mode]

Modes:
  --linux                 Build/run the Linux desktop app
  --android-setup         Install or repair Android SDK, NDK, emulator, and Qt Android kits
  --android               Build, install, and launch the Android APK on the emulator

Legacy aliases:
  --android-build         Build the Android APK only
  --android-run           Same as --android
  --android-setup-build   Install Android dependencies, then build the APK
  --setup-android-deps    Same as --android-setup-build
  --skip-android-deps     Same as --linux
EOF
}

for arg in "$@"; do
    case "${arg}" in
        --linux)
            mode="linux"
            ;;
        --android-setup)
            mode="android-setup"
            ;;
        --android)
            mode="android"
            ;;
        --android-build)
            mode="android-build"
            ;;
        --android-run)
            mode="android"
            ;;
        --android-setup-build|--setup-android-deps)
            mode="android-setup-build"
            ;;
        --skip-android-deps)
            mode="linux"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            pass_args+=("${arg}")
            ;;
    esac
done

choose_mode() {
    if [[ ! -t 0 ]]; then
        printf 'No build mode selected in noninteractive mode; defaulting to Linux. Use --android for Android.\n'
        mode="linux"
        return
    fi

    cat <<EOF
Select PrintFlow build target:
  1) Linux desktop build
  2) Android setup / repair environment
  3) Android build + install + run on emulator
  q) Quit
EOF
    printf 'Choice [1]: '
    read -r reply

    case "${reply:-1}" in
        1) mode="linux" ;;
        2) mode="android-setup" ;;
        3) mode="android" ;;
        q|Q) exit 0 ;;
        *) printf 'Unknown choice: %s\n' "${reply}" >&2; exit 1 ;;
    esac
}

[[ -n "${mode}" ]] || choose_mode

case "${mode}" in
    linux)
        exec "${SCRIPT_DIR}/scripts/dev_build_linux.sh" "${pass_args[@]}"
        ;;
    android-setup)
        exec "${SCRIPT_DIR}/scripts/install_android_dependencies.sh"
        ;;
    android-build)
        exec "${SCRIPT_DIR}/scripts/dev_build_android.sh" "${pass_args[@]}"
        ;;
    android)
        exec "${SCRIPT_DIR}/scripts/android_build_install_run.sh" "${pass_args[@]}"
        ;;
    android-setup-build)
        PRINTFLOW_ANDROID_SETUP_ASSUME_YES=1 "${SCRIPT_DIR}/scripts/install_android_dependencies.sh"
        exec "${SCRIPT_DIR}/scripts/dev_build_android.sh" "${pass_args[@]}"
        ;;
    *)
        printf 'Unknown mode: %s\n' "${mode}" >&2
        usage >&2
        exit 1
        ;;
esac
