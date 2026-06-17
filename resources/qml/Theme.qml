// Theme.qml
import QtQuick

QtObject {
    // Controlled by Main.qml (and later persisted if you want)
    property bool dark: true
    property var manager: themeManager

    // --- Dark palette (OpCharcoal + Teal) ---
    readonly property color _bgDark:       "#0F131A"
    readonly property color _surfaceDark:  "#14181F"
    readonly property color _surface2Dark: "#1A202A"
    readonly property color _textDark:     "#E6EAF2"
    readonly property color _subtextDark:  "#A7B0C0"
    readonly property color _dividerDark:  "#263042"

	// --- Light palette (soft, low-glare, professional) ---
	readonly property color _bgLight:       "#ECEFF4"  // soft gray-blue background
	readonly property color _surfaceLight:  "#F6F7FB"  // off-white surface (not pure)
	readonly property color _surface2Light: "#E3E7EF"  // stronger surface for cards/buttons
	readonly property color _textLight:     "#1F2937"  // softer than pure black
	readonly property color _subtextLight:  "#4B5563"  // good contrast
	readonly property color _dividerLight:  "#C7CEDB"  // stronger dividers

    // Accent (keep constant across themes)
    property color primary: manager ? manager.primaryColor : "#14181F"
    property color secondary: manager ? manager.secondaryColor : "#1FB8A6"
    property color accent: manager ? manager.accentColor : "#2DD4BF"
    property color accent2: secondary

    // Status (constant)
    property color danger:  "#EF4444"
    property color warning: "#F59E0B"

    // Public palette (switches based on dark flag)
    property color bg:       dark ? (manager ? manager.backgroundColor : _bgDark) : (manager ? manager.lightBackgroundColor : _bgLight)
    property color surface:  dark ? (manager ? manager.surfaceColor : _surfaceDark) : (manager ? manager.lightSurfaceColor : _surfaceLight)
    property color surface2: dark ? (manager ? manager.surface2Color : _surface2Dark) : (manager ? manager.lightSurface2Color : _surface2Light)
    property color text:     dark ? (manager ? manager.textColor : _textDark) : (manager ? manager.lightTextColor : _textLight)
    property color subtext:  dark ? (manager ? manager.subtextColor : _subtextDark) : (manager ? manager.lightSubtextColor : _subtextLight)
    property color divider:  dark ? (manager ? manager.dividerColor : _dividerDark) : (manager ? manager.lightDividerColor : _dividerLight)

    property string appName: manager ? manager.appName : "PrintFlow"
    property string displayName: manager ? manager.displayName : "Default"
    property string logoPath: manager && manager.logoPath.length > 0 ? manager.logoPath : "qrc:/assets/logo.png"
    property string splashLogoPath: manager && manager.splashLogoPath.length > 0 ? manager.splashLogoPath : logoPath
    property int logoWidth: manager && manager.logoWidth > 0 ? manager.logoWidth : 40
    property int logoHeight: manager && manager.logoHeight > 0 ? manager.logoHeight : 40
    property string aboutVendorName: manager ? manager.aboutVendorName : ""
    property string supportUrl: manager ? manager.supportUrl : ""
    property string copyrightText: manager ? manager.copyrightText : ""
}
