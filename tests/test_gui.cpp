#include <gtest/gtest.h>
#include "gui/Theme.h"

using namespace aios::gui;

// ============================================================================
// UI Theme & Skeuomorphism-Glassmorphism System Tests
// ============================================================================

TEST(ThemeTest, EnforcesFloatingIslandRadiusAndMargins) {
    // Top & Bottom Navigation Bars MUST be floating islands with capsule / pill shapes (corner_radius >= 24dp)
    EXPECT_GE(Theme::RADIUS_FLOATING_ISLAND, 24);
    
    // Elevated from screen edges with margins (12dp - 16dp)
    EXPECT_GE(Theme::MARGIN_FLOATING_ISLAND, 12);
    EXPECT_LE(Theme::MARGIN_FLOATING_ISLAND, 16);

    std::string island_css = Theme::getFloatingIslandCss();
    EXPECT_NE(island_css.find("border-radius: 28px"), std::string::npos);
    EXPECT_NE(island_css.find("margin: 14px"), std::string::npos);
    EXPECT_NE(island_css.find("rgba(28, 30, 44, 0.80)"), std::string::npos);
}

TEST(ThemeTest, EnforcesPillAndCircularButtonRadii) {
    // All interactive buttons, mode selectors, toggle icons MUST be circular or pill-shaped (corner_radius >= 20dp)
    EXPECT_GE(Theme::RADIUS_PILL_BUTTON, 20);

    // Large container cards should be rectanglish with deep rounded corners (corner_radius >= 14dp)
    EXPECT_GE(Theme::RADIUS_CONTAINER_CARD, 14);

    std::string pill_css = Theme::getPillButtonCss(true);
    EXPECT_NE(pill_css.find("border-radius: 22px"), std::string::npos);
    EXPECT_NE(pill_css.find("#FF6B9D"), std::string::npos);
    EXPECT_NE(pill_css.find("#FF9A56"), std::string::npos);

    std::string circ_css = Theme::getCircularButtonCss(44, true);
    EXPECT_NE(circ_css.find("border-radius: 22px"), std::string::npos);
}

TEST(ThemeTest, GeneratesBrandGradientsMatchingIdentity) {
    // Coral Rose (#FF6B9D) to Sunset Orange (#FF9A56) gradient
    std::string grad = Theme::getBrandGradientCss(false);
    EXPECT_NE(grad.find("#FF6B9D"), std::string::npos);
    EXPECT_NE(grad.find("#FF9A56"), std::string::npos);
}

TEST(ThemeTest, GeneratesComprehensiveGlobalStyleSheet) {
    std::string global_css = Theme::getGlobalStyleSheet();
    EXPECT_NE(global_css.find("QMainWindow"), std::string::npos);
    EXPECT_NE(global_css.find("QScrollBar"), std::string::npos);
    EXPECT_NE(global_css.find("QComboBox"), std::string::npos);
}
