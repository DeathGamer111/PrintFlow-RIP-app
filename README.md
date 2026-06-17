# RIP App

RIP App is a Qt 6 raster image processing application for preparing and outputting print jobs. It combines job management, image inspection/editing, imposition controls, ICC-based color conversion, stochastic screening, dot strategy tuning, and PRN generation for Nocai-style print workflows.

The current codebase is focused on Linux desktop development with CMake, CUPS, ImageMagick, and Little CMS.

## Current Status

- Active branch: `master`
- Build system: CMake
- UI framework: Qt Quick/QML
- Main development build script: `./Dev_Build_App.sh`
- Primary executable target: `appRIPPrinterApp`

## Features

- Print job management with Qt model roles and JSON persistence.
- Image loading, validation, metadata extraction, and PDF preview rendering through ImageMagick.
- Image editing tools for crop, rotate, flip, resize, color adjustment, blur, sepia, vignette, swirl, implode, text, rectangle drawing, undo, and redo.
- Imposition view for positioning artwork on the selected media.
- Printer setup flow for CUPS printers, simulated Nocai output, and X-36NC multi-ink output.
- ICC profile handling through Little CMS, including bundled CMYK and multi-ink output profiles.
- Color-management settings for default input/output profiles, printer-specific profiles, profile families, and persisted dot strategy settings.
- Nocai PRN generation with 2-bit dot classification, stochastic screening, and dot promotion controls.
- Multi-ink PRN generation with 4, 5, 6, 7, 8, and 10 channel ink layouts.
- Linearization support using bundled XML presets.
- Runtime asset preparation for bundled ICC profiles, linearization files, logo assets, and local blue-noise masks.

## Supported Ink Layouts

The multi-ink backend currently supports:

- 4 color: `Y M C K`
- 5 color: `Y M C K + White`
- 6 color: `Y M C K + Light Magenta + Light Cyan`
- 7 color: `Y M C K + Light Magenta + Light Cyan + White`
- 8 color: `Y M C K + Light Magenta + Light Cyan + Light Black + Light Light Black`
- 10 color: `Y M C K + Light Magenta + Light Cyan + Light Black + Light Light Black + White + Varnish`

The app defaults to the X-36NC photo-printer style multi-ink workflow and prepares the required runtime assets on startup.

## Project Layout

```text
.
|-- android/                       Qt Android package template
|-- docs/                          Flowchart and software design document
|-- packaging/linux/               Linux desktop/AppImage metadata
|-- resources/assets/              Bundled ICC profiles, linearization XML, and logo
|-- resources/qml/                 Qt Quick user interface
|-- scripts/                       Linux, Android, packaging, and policy helper scripts
|-- src/app/                       Application bootstrap
|-- src/core/                      Shared models, settings, assets, and capabilities
|-- src/platform/android/          Android-safe platform facades for APK boot
|-- src/platform/desktop/          Linux desktop integrations such as CUPS
|-- src/rip/                       Native RIP, color, screening, and PRN pipeline
|-- src/vendor/nocai/              Shared Nocai direct-print SDK loader/client
`-- src/vendor/stb/                Third-party single-header image loader
```

Important QML views include:

- `resources/qml/Main.qml`
- `resources/qml/JobListView.qml`
- `resources/qml/JobDetailsView.qml`
- `resources/qml/ImageEditorView.qml`
- `resources/qml/ImpositionView.qml`
- `resources/qml/PrinterSetupView.qml`
- `resources/qml/ColorManagementView.qml`

## Assets

Small runtime assets are tracked in `resources/assets/`, including:

- Output ICC profiles for 4-color, 8-color, 1440 plain default, 1440 plain neutral, and generic CMYK workflows.
- `sRGBProfile.icm`
- Linearization XML presets for 4-color and 8-color output.
- `logo.png`

Large blue-noise mask directories are intentionally ignored by Git:

```text
resources/assets/blue_noise_mask_*/**
```

For local builds that generate multi-ink output, the app expects the `resources/assets/blue_noise_mask_512_12000/` directory to exist locally with the mask TIFF files used by `scripts/dev_build_linux.sh`. The masks can be embedded into Qt resources with `-DRIP_EMBED_BLUE_NOISE_MASKS=ON`, but the default leaves them as local runtime assets to avoid very large generated resource objects.

## Requirements

The development script installs/checks the main Linux dependencies:

- CMake and a C++ compiler
- Qt 6 Quick, Widgets, Quick Controls 2, QML tooling, and related QML modules
- CUPS development libraries
- ImageMagick Magick++
- Little CMS 2
- AppImage/package helper tooling used by the local workflow

On Debian/Ubuntu-style systems, use the main development script when you want the full local setup path:

```bash
./Dev_Build_App.sh
```

The root script delegates to `scripts/dev_build_linux.sh`. It uses `sudo apt-get`, relaxes ImageMagick policy limits through `scripts/Relax_ImageMagick_Limits.sh`, clears the local app cache/build folder, runs CMake, builds the app, and copies blue-noise masks into:

```text
~/.local/share/appRIPPrinterApp/runtime_assets/
```

## Standard Build

For a normal compile without the dependency-install and policy steps:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Run the app with:

```bash
./build/appRIPPrinterApp
```

## Android Build

The first Android milestone is an APK that builds, installs, and boots in an emulator. Android builds default to boot-safe facades for CUPS and the native RIP pipeline. The shared Nocai direct-print SDK client is still compiled on Android and will package `libSYPrintAPIforPROII.so` when `DIRECT_PRINT_SDK_ROOT` points at a local ignored SDK folder containing that library.

Required environment variables:

```bash
export QT_ANDROID_CMAKE="$HOME/Qt/<version>/android_arm64_v8a/bin/qt-cmake"
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/<version>"
export DIRECT_PRINT_SDK_ROOT="$PWD/DemoForARM64Linux-260612/Demo260612"
```

Build the APK:

```bash
scripts/dev_build_android.sh
```

Run it on an emulator:

```bash
AVD_NAME=<your-avd-name> scripts/run_android_emulator.sh
```

## Development Notes

- `build/` is ignored and should not be committed.
- The generated Qt resource output under `.rcc/` is ignored.
- The blue-noise mask source directory is ignored because the masks are large local runtime assets.
- The tracked ICC and XML assets are required by the color-management and multi-ink paths.
- `Dev_Build_App.sh` is a compatibility wrapper around `scripts/dev_build_linux.sh`.
- `scripts/dev_build_android.sh` validates the Android toolchain and builds the APK target.
- `scripts/run_android_emulator.sh` installs and launches the latest built APK.

## Verification

A useful quick verification path is:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
timeout 8s ./build/appRIPPrinterApp
```

The timeout command is only a smoke test for startup and QML/runtime initialization; it stops the GUI after a few seconds.

## Documentation

Additional project documentation is in `docs/`:

- `docs/RIP-App_Software_Design_Document.docx`
- `docs/RIP-APP_Flowchart.drawio`
- `docs/RIP-APP_Flowchart.png`
- `docs/README.md`

## License

No license file is currently included in this repository.

## Author

Created and maintained by **DeathGamer111**.
