#pragma once

#include <string>

namespace aios {
namespace gui {

/**
 * @brief Mobile Skeuomorphism-Glassmorphism Theme System
 * Strict compliance with UI Design rules:
 * - Coral Rose (#FF6B9D) to Sunset Orange (#FF9A56) primary brand gradient
 * - Floating Island Navigation Bars with capsule / pill shapes (corner_radius >= 24px, 12-16px margin)
 * - Interactive buttons, mode selectors, toggle icons are circular or floating pill-shaped (corner_radius >= 20px)
 * - Container cards with deep rounded corners (corner_radius >= 14px)
 * - Tactile depth with a single virtual light source from top-left
 */
class Theme {
public:
    // Color Palette Constants
    static constexpr const char* COLOR_CORAL_ROSE   = "#FF6B9D";
    static constexpr const char* COLOR_SUNSET_ORANGE = "#FF9A56";
    static constexpr const char* COLOR_BG_DARK       = "#12131A";
    static constexpr const char* COLOR_SURFACE_GLASS = "rgba(28, 30, 44, 0.80)";
    static constexpr const char* COLOR_CARD_GLASS    = "rgba(36, 39, 58, 0.75)";
    static constexpr const char* COLOR_BORDER_LIGHT  = "rgba(255, 255, 255, 0.15)";
    static constexpr const char* COLOR_TEXT_PRIMARY  = "#FFFFFF";
    static constexpr const char* COLOR_TEXT_MUTED    = "#A0A5B8";
    static constexpr const char* COLOR_SUCCESS       = "#2DD4BF";
    static constexpr const char* COLOR_ERROR         = "#F87171";
    static constexpr const char* COLOR_WARNING       = "#FBBF24";

    // Corner Radii
    static constexpr int RADIUS_FLOATING_ISLAND = 28; // >= 24dp
    static constexpr int RADIUS_PILL_BUTTON     = 22; // >= 20dp
    static constexpr int RADIUS_CONTAINER_CARD  = 16; // >= 14dp
    static constexpr int MARGIN_FLOATING_ISLAND = 14; // 12dp - 16dp

    // Primary Brand Gradient (Top-left to bottom-right light source)
    static std::string getBrandGradientCss(bool pressed = false);
    static std::string getGlassBackgroundCss(int radius = RADIUS_CONTAINER_CARD);
    static std::string getFloatingIslandCss();
    static std::string getPillButtonCss(bool is_accent = false);
    static std::string getCircularButtonCss(int diameter = 44, bool is_accent = false);
    static std::string getContainerCardCss();
    static std::string getGlobalStyleSheet();
};

} // namespace gui
} // namespace aios
