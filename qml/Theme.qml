// Theme.qml
import QtQuick

QtObject {
    // Controlled by Main.qml (and later persisted if you want)
    property bool dark: true

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
    property color accent:  "#2DD4BF"
    property color accent2: "#1FB8A6"

    // Status (constant)
    property color danger:  "#EF4444"
    property color warning: "#F59E0B"

    // Public palette (switches based on dark flag)
    property color bg:       dark ? _bgDark       : _bgLight
    property color surface:  dark ? _surfaceDark  : _surfaceLight
    property color surface2: dark ? _surface2Dark : _surface2Light
    property color text:     dark ? _textDark     : _textLight
    property color subtext:  dark ? _subtextDark  : _subtextLight
    property color divider:  dark ? _dividerDark  : _dividerLight
}

