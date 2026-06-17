#!/bin/bash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${REPO_ROOT}/.android-sdk}"
ANDROID_NDK_VERSION="${ANDROID_NDK_VERSION:-29.0.13113456}"
QT_VERSION="${QT_VERSION:-6.8.3}"
QT_INSTALL_ROOT="${QT_INSTALL_ROOT:-${HOME}/Qt}"
QT_DESKTOP_TARGET="${QT_DESKTOP_TARGET:-linux_gcc_64}"
QT_ANDROID_TARGETS="${QT_ANDROID_TARGETS:-android_x86_64 android_arm64_v8a}"
QT_DESKTOP_MODULES="${QT_DESKTOP_MODULES:-}"
QT_ANDROID_MODULES="${QT_ANDROID_MODULES:-}"
AQT_VENV="${AQT_VENV:-${REPO_ROOT}/.tools/aqt}"
ANDROID_ENV_FILE="${ANDROID_ENV_FILE:-${REPO_ROOT}/.android-env}"

fail() {
    printf 'error: %s\n' "$1" >&2
    exit 1
}

info() {
    printf '       %s\n' "$1"
}

step() {
    printf '\n==> %s\n' "$1"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "Missing command after setup: $1"
}

first_qt_target() {
    # shellcheck disable=SC2086
    set -- ${QT_ANDROID_TARGETS}
    printf '%s' "$1"
}

qt_desktop_install_dir() {
    local candidate

    for candidate in \
        "${QT_INSTALL_ROOT}/${QT_VERSION}/gcc_64" \
        "${QT_INSTALL_ROOT}/${QT_VERSION}/${QT_DESKTOP_TARGET}"
    do
        if [[ -x "${candidate}/bin/qt-cmake" ]]; then
            printf '%s' "${candidate}"
            return
        fi
    done

    printf '%s' "${QT_INSTALL_ROOT}/${QT_VERSION}/${QT_DESKTOP_TARGET}"
}

install_host_packages() {
    step "Installing Linux host packages"
    info "sudo may ask for your password."

    sudo apt-get update
    sudo apt-get install -y \
        ca-certificates curl unzip zip tar xz-utils git \
        cmake ninja-build pkg-config \
        python3 python3-venv python3-pip \
        openjdk-17-jdk \
        qemu-kvm libvirt-daemon-system libvirt-clients bridge-utils cpu-checker \
        libgl1-mesa-dev libglu1-mesa libpulse0 \
        libxcb-cursor0 libxcb-xinerama0 libxcb-xkb1 libxkbcommon-x11-0 \
        libnss3 libxcomposite1 libxcursor1 libxi6 libxtst6 libxrandr2

    if getent group kvm >/dev/null 2>&1; then
        sudo usermod -aG kvm "${USER}" || true
        info "Added ${USER} to the kvm group if possible. Log out/in if /dev/kvm is still not writable."
    fi
}

install_android_sdk_and_avd() {
    step "Installing Android SDK, NDK, emulator, and Pixel AVD"
    ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT}" \
    ANDROID_NDK_VERSION="${ANDROID_NDK_VERSION}" \
        "${REPO_ROOT}/scripts/setup_android_emulator.sh"
}

install_aqt() {
    step "Preparing aqtinstall"
    python3 -m venv "${AQT_VENV}"
    "${AQT_VENV}/bin/python" -m pip install --upgrade pip
    "${AQT_VENV}/bin/python" -m pip install --upgrade aqtinstall
}

aqt_install_qt() {
    local host="$1"
    local target="$2"
    local qt_arch="$3"
    local modules="$4"
    local cmd

    cmd=("${AQT_VENV}/bin/python" -m aqt install-qt "${host}" "${target}" "${QT_VERSION}" "${qt_arch}" -O "${QT_INSTALL_ROOT}")
    if [[ -n "${modules}" ]]; then
        # shellcheck disable=SC2206
        module_args=(${modules})
        cmd+=(-m "${module_args[@]}")
    fi

    "${cmd[@]}"
}

install_qt_desktop_tools() {
    local qt_cmake qt_host_path

    step "Installing Qt ${QT_VERSION} desktop host tools"
    mkdir -p "${QT_INSTALL_ROOT}"

    qt_host_path="$(qt_desktop_install_dir)"
    qt_cmake="${qt_host_path}/bin/qt-cmake"
    if [[ -x "${qt_cmake}" ]]; then
        info "Qt desktop host tools already installed: ${qt_host_path}"
        return
    fi

    aqt_install_qt linux desktop "${QT_DESKTOP_TARGET}" "${QT_DESKTOP_MODULES}"
}

install_qt_android_targets() {
    local target qt_cmake

    step "Installing Qt ${QT_VERSION} Android kits"
    mkdir -p "${QT_INSTALL_ROOT}"

    for target in ${QT_ANDROID_TARGETS}; do
        qt_cmake="${QT_INSTALL_ROOT}/${QT_VERSION}/${target}/bin/qt-cmake"
        if [[ -x "${qt_cmake}" ]]; then
            info "Qt Android target already installed: ${target}"
            continue
        fi

        aqt_install_qt linux android "${target}" "${QT_ANDROID_MODULES}"
    done
}

write_env_file() {
    local default_target qt_android_cmake qt_host_path

    default_target="$(first_qt_target)"
    qt_android_cmake="${QT_INSTALL_ROOT}/${QT_VERSION}/${default_target}/bin/qt-cmake"
    qt_host_path="$(qt_desktop_install_dir)"

    [[ -x "${qt_android_cmake}" ]] || fail "Qt Android qt-cmake was not found: ${qt_android_cmake}"
    [[ -x "${qt_host_path}/bin/qt-cmake" ]] || fail "Qt host qt-cmake was not found: ${qt_host_path}/bin/qt-cmake"

    step "Writing Android build environment"
    cat > "${ANDROID_ENV_FILE}" <<EOF
# Source this file before Android builds:
#   source "${ANDROID_ENV_FILE}"
export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT}"
export ANDROID_NDK_ROOT="${ANDROID_SDK_ROOT}/ndk/${ANDROID_NDK_VERSION}"
export QT_ANDROID_CMAKE="${qt_android_cmake}"
export QT_HOST_PATH="${qt_host_path}"
export ANDROID_ABI="\${ANDROID_ABI:-x86_64}"
export GRADLE_USER_HOME="${REPO_ROOT}/.gradle"
export PATH="${ANDROID_SDK_ROOT}/platform-tools:${ANDROID_SDK_ROOT}/emulator:${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin:\$PATH"
EOF

    info "Environment file: ${ANDROID_ENV_FILE}"
}

verify_setup() {
    step "Verifying Android development tools"
    # shellcheck disable=SC1090
    source "${ANDROID_ENV_FILE}"

    require_command java
    require_command ninja
    require_command adb
    require_command emulator
    [[ -x "${QT_ANDROID_CMAKE}" ]] || fail "QT_ANDROID_CMAKE is not executable: ${QT_ANDROID_CMAKE}"
    [[ -x "${QT_HOST_PATH}/bin/qt-cmake" ]] || fail "QT_HOST_PATH does not contain bin/qt-cmake: ${QT_HOST_PATH}"
    [[ -d "${ANDROID_NDK_ROOT}" ]] || fail "ANDROID_NDK_ROOT does not exist: ${ANDROID_NDK_ROOT}"

    info "QT_ANDROID_CMAKE=${QT_ANDROID_CMAKE}"
    info "QT_HOST_PATH=${QT_HOST_PATH}"
    info "ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT}"
    info "ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT}"
}

cat <<EOF
This will install Android development dependencies for PrintFlow.

It may:
  - install Linux packages using sudo apt
  - download Android command-line tools, platform tools, emulator, NDK, and a Pixel system image
  - create/update ${ANDROID_SDK_ROOT}
  - install Qt ${QT_VERSION} desktop host tools and Android kits under ${QT_INSTALL_ROOT}
  - write ${ANDROID_ENV_FILE}

Override defaults with env vars such as QT_VERSION, QT_INSTALL_ROOT,
ANDROID_SDK_ROOT, ANDROID_NDK_VERSION, QT_DESKTOP_TARGET,
QT_ANDROID_TARGETS, QT_DESKTOP_MODULES, or QT_ANDROID_MODULES.
EOF

if [[ "${PRINTFLOW_ANDROID_SETUP_ASSUME_YES:-0}" == "1" ]]; then
    info "PRINTFLOW_ANDROID_SETUP_ASSUME_YES=1; continuing without prompt."
elif [[ -t 0 ]]; then
    printf '\nContinue? [y/N] '
    read -r reply
    case "${reply}" in
        y|Y|yes|YES|Yes) ;;
        *) printf 'Android dependency setup skipped.\n'; exit 0 ;;
    esac
else
    printf 'Android dependency setup requires confirmation. Re-run from a terminal or set PRINTFLOW_ANDROID_SETUP_ASSUME_YES=1.\n'
    exit 0
fi

install_host_packages
install_android_sdk_and_avd
install_aqt
install_qt_desktop_tools
install_qt_android_targets
write_env_file
verify_setup

cat <<EOF

Android dependency setup complete.

Next steps:
  source "${ANDROID_ENV_FILE}"
  scripts/dev_build_android.sh
  scripts/android_build_install_run.sh
EOF
