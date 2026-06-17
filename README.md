# PrintFlow

PrintFlow is a Qt 6 raster image processing application for preparing and outputting print jobs. It combines job management, image inspection/editing, imposition controls, ICC-based color conversion, stochastic screening, dot strategy tuning, and PRN generation for production print workflows.

The current codebase is focused on Linux desktop development with CMake, CUPS, ImageMagick, and Little CMS.

## Current Status

- Active branch: `android-apk-prep`
- Build system: CMake
- UI framework: Qt Quick/QML
- Main development build script: `./Dev_Build_App.sh`
- Primary executable target: `PrintFlow`
- Base product identity: `PrintFlow`; customer/vendor display branding belongs in theme configuration.

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
|-- resources/assets/              Bundled ICC profiles, linearization XML, and logo
|-- resources/packaging/linux/     Linux desktop/AppImage metadata
|-- resources/qml/                 Qt Quick user interface
|-- scripts/                       Linux, Android, packaging, and policy helper scripts
|-- src/app/                       Application bootstrap
|-- src/core/                      Shared models, settings, assets, and capabilities
|-- src/platform/android/          Android-safe platform facades for APK boot
|-- src/platform/desktop/          Linux desktop integrations such as CUPS
|-- src/rip/                       Native RIP, color, screening, and PRN pipeline
|-- src/third_party/stb/           Third-party single-header image loader
`-- src/vendor/nocai/              Vendor integration adapter for direct-print support
```

## Tests

Run the local Linux test suite with:

```bash
scripts/run_tests.sh
```

The script configures `build-tests`, builds the app and test executables, then runs `ctest --output-on-failure`. Tests do not require `DIRECT_PRINT_SDK_ROOT`, blue-noise mask fixtures, or an Android device.

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
~/.local/share/PrintFlow/runtime_assets/
```

## Standard Build

For a normal compile without the dependency-install and policy steps:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Run the app with:

```bash
./build/PrintFlow
```

## Theme Builds

PrintFlow supports build-time theme selection for normal/basic and custom-branded builds. If no theme variable is set, the default/basic theme is used.

Built-in themes:

- `default`: neutral PrintFlow branding.
- `nocai`: Nocai-oriented colors using public brand cues from Nocai printer materials.
- `xante`: Xante/iQueue-oriented colors using public Xante and iQueue software cues.

Build with a built-in theme:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-nocai -DCMAKE_BUILD_TYPE=Debug -DRIP_THEME=nocai
cmake -S . -B build-xante -DCMAKE_BUILD_TYPE=Debug -DRIP_THEME=xante
```

The development scripts forward these environment variables to CMake:

```bash
RIP_THEME=nocai scripts/dev_build_linux.sh
RIP_THEME=xante scripts/dev_build_android.sh
```

Custom theme files can be embedded at configure time:

```bash
RIP_THEME_FILE=/path/to/custom-theme.json scripts/dev_build_linux.sh
cmake -S . -B build-custom -DCMAKE_BUILD_TYPE=Debug -DRIP_THEME_FILE=/path/to/custom-theme.json
```

Custom theme JSON must include `id`, `displayName`, and `appName`. Other fields fall back to the default theme when omitted:

```json
{
  "id": "customer",
  "displayName": "Customer Theme",
  "appName": "Customer PrintFlow",
  "primaryColor": "#14181F",
  "secondaryColor": "#1FB8A6",
  "backgroundColor": "#0F131A",
  "surfaceColor": "#14181F",
  "surface2Color": "#1A202A",
  "textColor": "#E6EAF2",
  "subtextColor": "#A7B0C0",
  "dividerColor": "#263042",
  "lightBackgroundColor": "#ECEFF4",
  "lightSurfaceColor": "#F6F7FB",
  "lightSurface2Color": "#E3E7EF",
  "lightTextColor": "#1F2937",
  "lightSubtextColor": "#4B5563",
  "lightDividerColor": "#C7CEDB",
  "accentColor": "#2DD4BF",
  "logoPath": "qrc:/themes/customer/assets/logo.png",
  "splashLogoPath": "qrc:/themes/customer/assets/splash.png",
  "logoWidth": 96,
  "logoHeight": 40,
  "aboutVendorName": "Customer",
  "supportUrl": "https://example.com/support",
  "copyrightText": "Copyright (c) 2026 Customer"
}
```

Theme assets may live under `resources/themes/<theme-id>/assets/` or `resources/vendor/<vendor-id>/assets/` and can be referenced with `qrc:/...` paths. Custom file builds fail during CMake configure when the file is missing, invalid JSON, or missing required identity fields.

## Android Build

The first Android milestone is an APK that builds, installs, and boots in an emulator. Android builds default to boot-safe facades for CUPS and the native RIP pipeline. The shared direct-print SDK client is still compiled on Android and will package the vendor direct-print SDK library when `DIRECT_PRINT_SDK_ROOT` points at a local ignored SDK folder containing it.

Required environment variables:

```bash
export QT_ANDROID_CMAKE="$HOME/Qt/<version>/android_x86_64/bin/qt-cmake"
export ANDROID_SDK_ROOT="$PWD/.android-sdk"
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/<version>"
export DIRECT_PRINT_SDK_ROOT="/path/to/local/vendor/sdk/drop"
```

For emulator builds on a Linux workstation, point `QT_ANDROID_CMAKE` at a Qt Android x86_64 kit and use the default `ANDROID_ABI=x86_64`. For physical-device direct-print builds, use a Qt Android arm64 kit and set `ANDROID_ABI=arm64-v8a`.

Install the local Android SDK command-line tools, emulator packages, and a Pixel-style AVD:

```bash
scripts/setup_android_emulator.sh
```

Build the APK:

```bash
scripts/dev_build_android.sh
```

Build, install, and launch it on the emulator:

```bash
scripts/android_build_install_run.sh
```

The Android build defaults to `ANDROID_ABI=x86_64` for emulator testing on Linux. The x86_64 emulator requires KVM/VM acceleration with writable `/dev/kvm`; without it, `scripts/start_android_emulator.sh` and `scripts/run_android_emulator.sh` fail early with a host-setup message. Use `ANDROID_ABI=arm64-v8a` for physical-device builds that package the vendor direct-print SDK.

## Development Notes

- `build/` is ignored and should not be committed.
- The generated Qt resource output under `.rcc/` is ignored.
- The blue-noise mask source directory is ignored because the masks are large local runtime assets.
- The tracked ICC and XML assets are required by the color-management and multi-ink paths.
- `Dev_Build_App.sh` is a compatibility wrapper around `scripts/dev_build_linux.sh`.
- `scripts/setup_android_emulator.sh` installs local Android SDK/emulator packages and creates the default AVD.
- `scripts/dev_build_android.sh` validates the Android toolchain and builds the APK target.
- `scripts/start_android_emulator.sh` starts the configured AVD without requiring an APK.
- `scripts/run_android_emulator.sh` installs and launches the latest built APK.
- `scripts/android_build_install_run.sh` builds, installs, and launches in one step.

## Verification

A useful quick verification path is:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
timeout 8s ./build/PrintFlow
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
