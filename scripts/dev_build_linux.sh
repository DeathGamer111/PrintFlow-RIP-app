#!/bin/bash

set -euo pipefail

TARGET_NAME="PrintFlow"
BUILD_DIR="${BUILD_DIR:-build}"
SDK_DIR_NAME="DemoForARM64Linux-260612/Demo260612"
SDK_SOURCE_ROOT="${DIRECT_PRINT_SDK_ROOT:-${SDK_DIR_NAME}}"
SDK_TARGET_ROOT="${BUILD_DIR}/${SDK_DIR_NAME}"
MASK_SOURCE_DIR="resources/assets/blue_noise_mask_512_12000"
RUNTIME_DIR="${HOME}/.local/share/PrintFlow/runtime_assets"

STEP=0
TOTAL_STEPS=7

step() {
    STEP=$((STEP + 1))
    printf '\n[%2d/%2d] %s\n' "${STEP}" "${TOTAL_STEPS}" "$1"
}

info() {
    printf '       %s\n' "$1"
}

fail() {
    printf '\nerror: %s\n' "$1" >&2
    exit 1
}

normalize_arch() {
    local raw
    raw="$(uname -m)"

    case "${raw}" in
        x86_64|amd64) printf 'x86_64' ;;
        aarch64|arm64) printf 'arm64' ;;
        i386|i686) printf 'x86' ;;
        *) printf '%s' "${raw}" ;;
    esac
}

socket_arch_for() {
    case "$1" in
        x86_64) printf 'x64' ;;
        arm64) printf 'arm64' ;;
        x86) printf 'x86' ;;
        *) printf '%s' "$1" ;;
    esac
}

find_print_api_for_arch() {
    local arch="$1"

    case "${arch}" in
        arm64)
            printf '%s/libSYPrintAPIforPROII.so' "${SDK_SOURCE_ROOT}"
            ;;
        x86_64)
            for candidate in \
                "${SDK_SOURCE_ROOT}/linux/x64/libSYPrintAPIforPROII.so" \
                "${SDK_SOURCE_ROOT}/x64/libSYPrintAPIforPROII.so" \
                "${SDK_SOURCE_ROOT}/libSYPrintAPIforPROII-x86_64.so"
            do
                if [[ -f "${candidate}" ]]; then
                    printf '%s' "${candidate}"
                    return
                fi
            done
            ;;
        x86)
            for candidate in \
                "${SDK_SOURCE_ROOT}/linux/x86/libSYPrintAPIforPROII.so" \
                "${SDK_SOURCE_ROOT}/x86/libSYPrintAPIforPROII.so" \
                "${SDK_SOURCE_ROOT}/libSYPrintAPIforPROII-x86.so"
            do
                if [[ -f "${candidate}" ]]; then
                    printf '%s' "${candidate}"
                    return
                fi
            done
            ;;
    esac
}

install_direct_print_sdk() {
    local arch socket_arch source_api source_socket target_socket_dir target_api

    arch="$(normalize_arch)"
    socket_arch="$(socket_arch_for "${arch}")"
    source_api="$(find_print_api_for_arch "${arch}")"
    source_socket="${SDK_SOURCE_ROOT}/PrinterSocketDLL/linux/${socket_arch}/PrinterSocket.so"
    target_socket_dir="${SDK_TARGET_ROOT}/PrinterSocketDLL/linux/${socket_arch}"
    target_api="${SDK_TARGET_ROOT}/libSYPrintAPIforPROII.so"

    info "Machine architecture: ${arch}"
    mkdir -p "${SDK_TARGET_ROOT}" "${target_socket_dir}"

    if [[ -n "${source_api}" && -f "${source_api}" ]]; then
        cp -f "${source_api}" "${target_api}"
        info "Installed print API: ${target_api}"
    elif [[ "${arch}" == "x86_64" ]]; then
        printf 'placeholder for future x86_64 libSYPrintAPIforPROII.so\n' > "${target_api}"
        info "Installed dummy x86_64 print API placeholder: ${target_api}"
    else
        fail "No direct-print API library found for architecture ${arch}"
    fi

    if [[ -f "${source_socket}" ]]; then
        cp -f "${source_socket}" "${target_socket_dir}/PrinterSocket.so"
        info "Installed socket library: ${target_socket_dir}/PrinterSocket.so"
    else
        info "Socket library not found for ${socket_arch}; direct SDK may not load on this architecture."
    fi
}

step "Checking Linux build dependencies"
info "sudo may ask for your password."
sudo apt-get update -qq
sudo apt-get install -y \
    cmake g++ qt6-base-dev qt6-base-private-dev qt6-declarative-dev \
    qt6-declarative-dev-tools qt6-tools-dev qt6-tools-dev-tools \
    qt6-l10n-tools qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qt-labs-platform \
    qml6-module-qtquick-dialogs qt6-wayland pkg-config \
    liblcms2-dev libcups2-dev libmagick++-6.q16-dev libqt6quick6 \
    wget git fuse patchelf desktop-file-utils libglib2.0-bin \
    libfuse2 zsync xz-utils libgl1-mesa-dev libopengl-dev libvulkan-dev \
    qt6-declarative-dev-tools qt6-qmltooling-plugins \
    qml6-module-qtquick-dialogs libqt6widgets6 qml6-module-qtpositioning \
    qml6-module-qtcore qml6-module-qtquick-window qml-module-qtquick-shapes \
    qt5-qmltooling-plugins qt6-image-formats-plugins libqt6widgets6 \
    libqt6svg6 libqt6svgwidgets6 qml6-module-qtqml-workerscript \
    qml6-module-qtquick-templates

step "Applying ImageMagick policy"
sudo bash ./scripts/Relax_ImageMagick_Limits.sh

step "Preparing build directories"
sudo rm -rf "${HOME}/.local/share/PrintFlow/"
sudo rm -rf "${HOME}/.cache/PrintFlow/"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
info "Build directory: ${BUILD_DIR}"

step "Configuring CMake"
cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug

step "Building ${TARGET_NAME}"
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

step "Installing runtime assets"
mkdir -p "${RUNTIME_DIR}"

if [[ -d "${MASK_SOURCE_DIR}" ]]; then
    cp -f "${MASK_SOURCE_DIR}/mask_c.tiff"   "${RUNTIME_DIR}/mask_512_c.tiff"
    cp -f "${MASK_SOURCE_DIR}/mask_m.tiff"   "${RUNTIME_DIR}/mask_512_m.tiff"
    cp -f "${MASK_SOURCE_DIR}/mask_y.tiff"   "${RUNTIME_DIR}/mask_512_y.tiff"
    cp -f "${MASK_SOURCE_DIR}/mask_k.tiff"   "${RUNTIME_DIR}/mask_512_k.tiff"
    cp -f "${MASK_SOURCE_DIR}/mask_lc.tiff"  "${RUNTIME_DIR}/mask_512_lc.tiff"
    cp -f "${MASK_SOURCE_DIR}/mask_lm.tiff"  "${RUNTIME_DIR}/mask_512_lm.tiff"
    cp -f "${MASK_SOURCE_DIR}/mask_lk.tiff"  "${RUNTIME_DIR}/mask_512_lk.tiff"
    cp -f "${MASK_SOURCE_DIR}/mask_llk.tiff" "${RUNTIME_DIR}/mask_512_llk.tiff"
    cp -f "${MASK_SOURCE_DIR}/mask_w.tiff"   "${RUNTIME_DIR}/mask_512_w.tiff"
    cp -f "${MASK_SOURCE_DIR}/mask_v.tiff"   "${RUNTIME_DIR}/mask_512_v.tiff"
    info "Runtime masks: ${RUNTIME_DIR}"
else
    info "Mask directory not found: ${MASK_SOURCE_DIR}"
fi

install_direct_print_sdk

step "Build complete"
info "Run: ./${BUILD_DIR}/${TARGET_NAME}"
info "SDK root: ${SDK_TARGET_ROOT}"
