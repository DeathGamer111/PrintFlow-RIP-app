cmake_minimum_required(VERSION 3.16)

project(PrintFlow VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RIP_ANDROID_ENABLE_RIP_PROCESSING "Build Android with the native RIP dependency stack instead of boot-safe stubs." OFF)
option(RIP_EMBED_BLUE_NOISE_MASKS "Embed local blue-noise TIFF masks into the Qt resource bundle." OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Concurrent Widgets QuickControls2 Xml)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(RIP_CORE_SOURCES
    src/app/main.cpp
    src/core/AppStrings.h src/core/AppStrings.cpp
    src/core/AssetManager.h src/core/AssetManager.cpp
    src/core/ColorManagementManager.h src/core/ColorManagementManager.cpp
    src/core/IPrintOutputClient.h
    src/core/PlatformCapabilities.h src/core/PlatformCapabilities.cpp
    src/core/PrintJob.h
    src/core/PrintJobModel.h src/core/PrintJobModel.cpp
    src/vendor/nocai/NocaiDirectPrintClient.h src/vendor/nocai/NocaiDirectPrintClient.cpp
    src/vendor/nocai/NocaiPrnWriter.h src/vendor/nocai/NocaiPrnWriter.cpp
)

set(RIP_NATIVE_SOURCES
    src/rip/ColorProfile.h src/rip/ColorProfile.cpp
    src/rip/ImageEditor.h src/rip/ImageEditor.cpp
    src/rip/ImageLoader.h src/rip/ImageLoader.cpp
    src/rip/MultiInkLinearization.h src/rip/MultiInkLinearization.cpp
    src/rip/MultiInkScreenEngine.h src/rip/MultiInkScreenEngine.cpp
    src/rip/MultiInkToneBuilder.h src/rip/MultiInkToneBuilder.cpp
    src/rip/MultiInkTypes.h
    src/rip/PrintJobMultiInk.h src/rip/PrintJobMultiInk.cpp
    src/rip/PrintJobCMYK.h src/rip/PrintJobCMYK.cpp
)

set(RIP_ANDROID_STUB_SOURCES
    src/platform/android/ColorProfile.h src/platform/android/ColorProfile.cpp
    src/platform/android/ImageEditor.h src/platform/android/ImageEditor.cpp
    src/platform/android/ImageLoader.h src/platform/android/ImageLoader.cpp
    src/platform/android/PrintJobMultiInk.h src/platform/android/PrintJobMultiInk.cpp
    src/platform/android/PrintJobCMYK.h src/platform/android/PrintJobCMYK.cpp
)

if(ANDROID AND NOT RIP_ANDROID_ENABLE_RIP_PROCESSING)
    set(RIP_PROCESSING_SOURCES ${RIP_ANDROID_STUB_SOURCES})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ImageMagick REQUIRED IMPORTED_TARGET Magick++)
    pkg_check_modules(LCMS REQUIRED IMPORTED_TARGET lcms2)
    set(RIP_PROCESSING_SOURCES ${RIP_NATIVE_SOURCES})
endif()

if(ANDROID)
    set(RIP_PLATFORM_SOURCES
        src/platform/android/PrintJobOutput.h src/platform/android/PrintJobOutput.cpp
    )
else()
    set(RIP_PLATFORM_SOURCES
        src/platform/desktop/PrintJobOutput.h src/platform/desktop/PrintJobOutput.cpp
    )
endif()

qt_add_executable(PrintFlow
    ${RIP_CORE_SOURCES}
    ${RIP_PROCESSING_SOURCES}
    ${RIP_PLATFORM_SOURCES}
    src/vendor/stb/stb_image.h
)

target_include_directories(PrintFlow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    src/app
    src/core
    src/vendor/nocai
    src/vendor/stb
)

if(ANDROID AND NOT RIP_ANDROID_ENABLE_RIP_PROCESSING)
    target_include_directories(PrintFlow PRIVATE src/platform/android)
else()
    target_include_directories(PrintFlow PRIVATE src/rip)
endif()

if(ANDROID)
    target_include_directories(PrintFlow PRIVATE src/platform/android)
else()
    target_include_directories(PrintFlow PRIVATE src/platform/desktop)
endif()

target_link_libraries(PrintFlow
    PRIVATE
        Qt6::Quick
        Qt6::Concurrent
        Qt6::QuickControls2
        Qt6::Widgets
        Qt6::Xml
)

if(NOT ANDROID)
    find_path(CUPS_INCLUDE_DIR cups/cups.h PATH_SUFFIXES cups)
    find_library(CUPS_LIBRARY NAMES cups)

    if(CUPS_INCLUDE_DIR AND CUPS_LIBRARY)
        message(STATUS "CUPS found: ${CUPS_LIBRARY}")
        target_include_directories(PrintFlow PRIVATE ${CUPS_INCLUDE_DIR})
        target_link_libraries(PrintFlow PRIVATE ${CUPS_LIBRARY})
    else()
        message(FATAL_ERROR "CUPS library not found")
    endif()
endif()

if(NOT ANDROID OR RIP_ANDROID_ENABLE_RIP_PROCESSING)
    target_link_libraries(PrintFlow
        PRIVATE
            PkgConfig::ImageMagick
            PkgConfig::LCMS
    )
endif()

if(ANDROID AND RIP_ANDROID_ENABLE_RIP_PROCESSING)
    target_compile_definitions(PrintFlow PRIVATE RIP_ANDROID_ENABLE_RIP_PROCESSING=1)
endif()

set(DIRECT_PRINT_SDK_ROOT "$ENV{DIRECT_PRINT_SDK_ROOT}" CACHE PATH "Local vendor direct-print SDK root. This path is intentionally not committed.")
if(NOT DIRECT_PRINT_SDK_ROOT)
    set(DIRECT_PRINT_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/DemoForARM64Linux-260612/Demo260612")
endif()

if(ANDROID)
    set(DIRECT_PRINT_SDK_LIBRARY "${DIRECT_PRINT_SDK_ROOT}/libSYPrintAPIforPROII.so")
    if(EXISTS "${DIRECT_PRINT_SDK_LIBRARY}")
        message(STATUS "Android direct-print SDK library: ${DIRECT_PRINT_SDK_LIBRARY}")
        set_property(TARGET PrintFlow APPEND PROPERTY QT_ANDROID_EXTRA_LIBS "${DIRECT_PRINT_SDK_LIBRARY}")
    else()
        message(WARNING "Android direct-print SDK library not found at ${DIRECT_PRINT_SDK_LIBRARY}. The APK can still boot, but direct print will report unavailable.")
    endif()

    set_target_properties(PrintFlow PROPERTIES
        QT_ANDROID_APP_NAME "PrintFlow"
        QT_ANDROID_PACKAGE_NAME "com.ripapp.printer"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()

file(GLOB RIP_QML_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/qml/*.qml"
)

set(RIP_ASSET_FILES
    resources/assets/logo.png
    resources/assets/RIP_App_1440_Plain_Default.icc
    resources/assets/RIP_App_1440_Plain_Neutral.icc
    resources/assets/RIP_App_1200_8C.icc
    resources/assets/RIP_App_1200_4C.icc
    resources/assets/RIP_App_Linearization_1200_4C.xml
    resources/assets/RIP_App_Linearization_1200_8C.xml
    resources/assets/RIP_App_Generic_CMYK.icc
    resources/assets/sRGBProfile.icm
)

set(RIP_I18N_FILES
    resources/i18n/strings.json
    resources/i18n/strings.zh-Hans.json
)

if(RIP_EMBED_BLUE_NOISE_MASKS)
    file(GLOB RIP_BLUE_NOISE_MASKS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/assets/blue_noise_mask_512_12000/*.tiff"
    )

    if(RIP_BLUE_NOISE_MASKS)
        list(APPEND RIP_ASSET_FILES ${RIP_BLUE_NOISE_MASKS})
    else()
        message(WARNING "Blue-noise mask TIFFs were not found under resources/assets/blue_noise_mask_512_12000.")
    endif()
else()
    message(STATUS "Blue-noise mask resource embedding is disabled. Set RIP_EMBED_BLUE_NOISE_MASKS=ON to package local masks into the Qt resource bundle.")
endif()

qt_add_resources(PrintFlow "app_resources"
    PREFIX "/"
    BASE "resources"
    FILES
        ${RIP_QML_FILES}
        ${RIP_ASSET_FILES}
        ${RIP_I18N_FILES}
)

set_target_properties(PrintFlow PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS PrintFlow
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(NOT ANDROID)
    enable_testing()

    add_executable(PrintFlowDirectPrintClientTest
        tests/DirectPrintClientUnavailableTest.cpp
        src/core/IPrintOutputClient.h
        src/vendor/nocai/NocaiDirectPrintClient.h
        src/vendor/nocai/NocaiDirectPrintClient.cpp
    )

    target_include_directories(PrintFlowDirectPrintClientTest PRIVATE
        src/core
        src/vendor/nocai
    )

    target_link_libraries(PrintFlowDirectPrintClientTest
        PRIVATE
            Qt6::Core
    )

    add_test(NAME PrintFlowDirectPrintClientUnavailable
             COMMAND PrintFlowDirectPrintClientTest)

    add_executable(PrintFlowNocaiPrnWriterTest
        tests/NocaiPrnWriterTest.cpp
        src/vendor/nocai/NocaiPrnWriter.h
        src/vendor/nocai/NocaiPrnWriter.cpp
    )

    target_include_directories(PrintFlowNocaiPrnWriterTest PRIVATE
        src/vendor/nocai
    )

    target_link_libraries(PrintFlowNocaiPrnWriterTest
        PRIVATE
            Qt6::Core
    )

    add_test(NAME PrintFlowNocaiPrnWriter
             COMMAND PrintFlowNocaiPrnWriterTest)

    add_executable(PrintFlowCMYKAssetManagerTest
        tests/PrintJobCMYKAssetManagerTest.cpp
        src/core/AssetManager.h src/core/AssetManager.cpp
        src/core/ColorManagementManager.h src/core/ColorManagementManager.cpp
        src/rip/PrintJobCMYK.h src/rip/PrintJobCMYK.cpp
        src/vendor/nocai/NocaiPrnWriter.h src/vendor/nocai/NocaiPrnWriter.cpp
    )

    qt_add_resources(PrintFlowCMYKAssetManagerTest "cmyk_asset_test_resources"
        PREFIX "/"
        BASE "resources"
        FILES
            ${RIP_ASSET_FILES}
    )

    target_include_directories(PrintFlowCMYKAssetManagerTest PRIVATE
        src/core
        src/rip
        src/vendor/nocai
    )

    target_link_libraries(PrintFlowCMYKAssetManagerTest
        PRIVATE
            Qt6::Core
            Qt6::Concurrent
            Qt6::Xml
            PkgConfig::ImageMagick
            PkgConfig::LCMS
    )

    add_test(NAME PrintFlowCMYKAssetManager
             COMMAND PrintFlowCMYKAssetManagerTest)
endif()
