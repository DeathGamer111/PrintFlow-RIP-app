#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

setup_android_deps=ask
linux_args=()

for arg in "$@"; do
    case "${arg}" in
        --setup-android-deps)
            setup_android_deps=yes
            ;;
        --skip-android-deps)
            setup_android_deps=no
            ;;
        *)
            linux_args+=("${arg}")
            ;;
    esac
done

if [[ "${setup_android_deps}" == "ask" ]]; then
    if [[ -t 0 && -z "${PRINTFLOW_SKIP_ANDROID_DEPS_PROMPT:-}" ]]; then
        printf 'Download and install Android development dependencies now? [y/N] '
        read -r reply
        case "${reply}" in
            y|Y|yes|YES|Yes) setup_android_deps=yes ;;
            *) setup_android_deps=no ;;
        esac
    else
        setup_android_deps=no
    fi
fi

if [[ "${setup_android_deps}" == "yes" ]]; then
    PRINTFLOW_ANDROID_SETUP_ASSUME_YES=1 "${SCRIPT_DIR}/scripts/install_android_dependencies.sh"
fi

exec "${SCRIPT_DIR}/scripts/dev_build_linux.sh" "${linux_args[@]}"
