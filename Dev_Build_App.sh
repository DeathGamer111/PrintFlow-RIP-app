#!/bin/bash

set -euo pipefail

APP_NAME="RIP_App"
BUILD_DIR="build"

echo "🔧 Installing required build dependencies (if not already installed)..."
sudo apt-get update
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
    
# === Run ImageMagick policy relaxer ===
echo "🔓 Relaxing ImageMagick security limits..."
sudo bash ./scripts/Relax_ImageMagick_Limits.sh

echo "🧹 Cleaning and creating build directory..."
sudo rm -rf ~/.local/share/appRIPPrinterApp/
sudo rm -rf ~/.cache/appRIPPrinterApp/
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "⚙️ Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Debug

echo "🛠️ Building the app..."
make -j"$(nproc)"

echo "📦 Copying blue-noise masks into runtime_assets..."

SRC_MASK_DIR="../assets/blue_noise_mask_512_12000"
RUNTIME_DIR="${HOME}/.local/share/appRIPPrinterApp/runtime_assets"

mkdir -p "$RUNTIME_DIR"

# Copy all mask_*.tiff files and rename them to the names the backend expects
cp -f "$SRC_MASK_DIR/mask_c.tiff"   "$RUNTIME_DIR/mask_512_c.tiff"
cp -f "$SRC_MASK_DIR/mask_m.tiff"   "$RUNTIME_DIR/mask_512_m.tiff"
cp -f "$SRC_MASK_DIR/mask_y.tiff"   "$RUNTIME_DIR/mask_512_y.tiff"
cp -f "$SRC_MASK_DIR/mask_k.tiff"   "$RUNTIME_DIR/mask_512_k.tiff"
cp -f "$SRC_MASK_DIR/mask_lc.tiff"  "$RUNTIME_DIR/mask_512_lc.tiff"
cp -f "$SRC_MASK_DIR/mask_lm.tiff"  "$RUNTIME_DIR/mask_512_lm.tiff"
cp -f "$SRC_MASK_DIR/mask_lk.tiff"  "$RUNTIME_DIR/mask_512_lk.tiff"
cp -f "$SRC_MASK_DIR/mask_llk.tiff" "$RUNTIME_DIR/mask_512_llk.tiff"
cp -f "$SRC_MASK_DIR/mask_w.tiff"   "$RUNTIME_DIR/mask_512_w.tiff"
cp -f "$SRC_MASK_DIR/mask_v.tiff"   "$RUNTIME_DIR/mask_512_v.tiff"

echo "✔ Masks copied to: $RUNTIME_DIR"
ls -lh "$RUNTIME_DIR"/mask_512_*.tiff

echo "✅ Build complete. Run the app using:"
echo "   cd ${BUILD_DIR} && ./appRIPPrinterApp"
echo "   or use /build/appRIPPrinterApp"
