# PrintFlow Documentation

This directory contains supporting design and workflow material for PrintFlow.

## Current App Shape

PrintFlow is a Qt 6 C++/QML application targeting Linux desktop first, with Android APK support being prepared on the `android-apk-prep` branch.

The base product identity is vendor-neutral: `PrintFlow`. Customer/vendor variants such as `Xante PrintFlow`, `Nocai PrintFlow`, or `iQueue PrintFlow` should be handled through theme configuration rather than by renaming the base target.

The source tree is split by responsibility:

- `src/core/`: shared job models, settings, asset helpers, and platform capability flags.
- `src/rip/`: native image processing, color conversion, screening, and PRN generation.
- `src/platform/desktop/`: Linux desktop integrations such as CUPS.
- `src/platform/android/`: Android-safe facades used for APK boot while native RIP dependencies are ported.
- `src/vendor/nocai/`: shared direct-print SDK loader/client.
- `src/vendor/stb/`: vendored single-header image loader.
- `resources/qml/` and `resources/assets/`: Qt Quick UI and bundled profiles/assets.
- `packaging/linux/`: Linux desktop packaging metadata.
- `android/`: Qt Android package template.

## Build Notes

Linux builds use CMake with Qt, CUPS, Magick++, and Little CMS:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Android builds use Qt's Android `qt-cmake` wrapper and the `apk` target through `scripts/dev_build_android.sh`. The first milestone is APK boot in an emulator; direct-print SDK packaging is supported when `DIRECT_PRINT_SDK_ROOT` points to a local ignored SDK folder containing `libSYPrintAPIforPROII.so`.

## Files

- `RIP-App_Software_Design_Document.docx`: design document.
- `RIP-APP_Flowchart.drawio`: editable workflow diagram.
- `RIP-APP_Flowchart.png`: exported workflow diagram.
