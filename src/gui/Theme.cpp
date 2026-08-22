#include "gui/Theme.h"
#include <sstream>

namespace aios {
namespace gui {

std::string Theme::getBrandGradientCss(bool pressed) {
    if (pressed) {
        return "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #E55A8A, stop:1 #E08544)";
    }
    return "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #FF6B9D, stop:1 #FF9A56)";
}

std::string Theme::getFloatingIslandCss() {
    std::ostringstream ss;
    ss << "background-color: " << COLOR_SURFACE_GLASS << ";\n"
       << "border-radius: " << RADIUS_FLOATING_ISLAND << "px;\n"
       << "border: 1px solid " << COLOR_BORDER_LIGHT << ";\n"
       << "margin: " << MARGIN_FLOATING_ISLAND << "px;\n";
    return ss.str();
}

std::string Theme::getGlassBackgroundCss(int radius) {
    std::ostringstream ss;
    ss << "background-color: " << COLOR_SURFACE_GLASS << ";\n"
       << "border-radius: " << radius << "px;\n"
       << "border: 1px solid " << COLOR_BORDER_LIGHT << ";\n";
    return ss.str();
}

std::string Theme::getPillButtonCss(bool is_accent) {
    std::ostringstream ss;
    if (is_accent) {
        ss << "QPushButton {\n"
           << "  background: " << getBrandGradientCss(false) << ";\n"
           << "  color: #FFFFFF;\n"
           << "  border-radius: " << RADIUS_PILL_BUTTON << "px;\n"
           << "  border: 1px solid rgba(255, 255, 255, 0.40);\n"
           << "  font-weight: bold;\n"
           << "  padding: 8px 18px;\n"
           << "}\n"
           << "QPushButton:hover {\n"
           << "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #FF7CA9, stop:1 #FFA76B);\n"
           << "}\n"
           << "QPushButton:pressed {\n"
           << "  background: " << getBrandGradientCss(true) << ";\n"
           << "}\n";
    } else {
        ss << "QPushButton {\n"
           << "  background: rgba(255, 255, 255, 0.08);\n"
           << "  color: " << COLOR_TEXT_PRIMARY << ";\n"
           << "  border-radius: " << RADIUS_PILL_BUTTON << "px;\n"
           << "  border: 1px solid " << COLOR_BORDER_LIGHT << ";\n"
           << "  padding: 8px 18px;\n"
           << "}\n"
           << "QPushButton:hover {\n"
           << "  background: rgba(255, 255, 255, 0.16);\n"
           << "  border-color: rgba(255, 255, 255, 0.35);\n"
           << "}\n"
           << "QPushButton:pressed {\n"
           << "  background: rgba(255, 255, 255, 0.04);\n"
           << "}\n";
    }
    return ss.str();
}

std::string Theme::getCircularButtonCss(int diameter, bool is_accent) {
    int radius = diameter / 2;
    std::ostringstream ss;
    if (is_accent) {
        ss << "QPushButton {\n"
           << "  background: " << getBrandGradientCss(false) << ";\n"
           << "  color: #FFFFFF;\n"
           << "  min-width: " << diameter << "px;\n"
           << "  max-width: " << diameter << "px;\n"
           << "  min-height: " << diameter << "px;\n"
           << "  max-height: " << diameter << "px;\n"
           << "  border-radius: " << radius << "px;\n"
           << "  border: 1px solid rgba(255, 255, 255, 0.40);\n"
           << "  font-size: 16px;\n"
           << "  font-weight: bold;\n"
           << "}\n"
           << "QPushButton:hover {\n"
           << "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #FF7CA9, stop:1 #FFA76B);\n"
           << "}\n"
           << "QPushButton:pressed {\n"
           << "  background: " << getBrandGradientCss(true) << ";\n"
           << "}\n";
    } else {
        ss << "QPushButton {\n"
           << "  background: rgba(255, 255, 255, 0.10);\n"
           << "  color: " << COLOR_TEXT_PRIMARY << ";\n"
           << "  min-width: " << diameter << "px;\n"
           << "  max-width: " << diameter << "px;\n"
           << "  min-height: " << diameter << "px;\n"
           << "  max-height: " << diameter << "px;\n"
           << "  border-radius: " << radius << "px;\n"
           << "  border: 1px solid " << COLOR_BORDER_LIGHT << ";\n"
           << "}\n"
           << "QPushButton:hover {\n"
           << "  background: rgba(255, 255, 255, 0.20);\n"
           << "}\n";
    }
    return ss.str();
}

std::string Theme::getContainerCardCss() {
    std::ostringstream ss;
    ss << "background-color: " << COLOR_CARD_GLASS << ";\n"
       << "border-radius: " << RADIUS_CONTAINER_CARD << "px;\n"
       << "border: 1px solid " << COLOR_BORDER_LIGHT << ";\n"
       << "padding: 12px;\n";
    return ss.str();
}

std::string Theme::getGlobalStyleSheet() {
    std::ostringstream ss;
    ss << "QMainWindow, QWidget#centralWidget {\n"
       << "  background-color: " << COLOR_BG_DARK << ";\n"
       << "  color: " << COLOR_TEXT_PRIMARY << ";\n"
       << "  font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, Roboto, sans-serif;\n"
       << "}\n\n"
       
       << "QLabel {\n"
       << "  color: " << COLOR_TEXT_PRIMARY << ";\n"
       << "}\n\n"

       << "QLineEdit, QTextEdit, QPlainTextEdit {\n"
       << "  background-color: rgba(20, 22, 32, 0.85);\n"
       << "  color: " << COLOR_TEXT_PRIMARY << ";\n"
       << "  border-radius: " << RADIUS_CONTAINER_CARD << "px;\n"
       << "  border: 1px solid " << COLOR_BORDER_LIGHT << ";\n"
       << "  padding: 10px 14px;\n"
       << "  selection-background-color: " << COLOR_CORAL_ROSE << ";\n"
       << "}\n"
       << "QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {\n"
       << "  border: 1px solid " << COLOR_CORAL_ROSE << ";\n"
       << "}\n\n"

       << "QScrollBar:vertical {\n"
       << "  border: none;\n"
       << "  background: rgba(0, 0, 0, 0.15);\n"
       << "  width: 8px;\n"
       << "  border-radius: 4px;\n"
       << "  margin: 0px;\n"
       << "}\n"
       << "QScrollBar::handle:vertical {\n"
       << "  background: rgba(255, 255, 255, 0.25);\n"
       << "  min-height: 24px;\n"
       << "  border-radius: 4px;\n"
       << "}\n"
       << "QScrollBar::handle:vertical:hover {\n"
       << "  background: " << COLOR_CORAL_ROSE << ";\n"
       << "}\n"
       << "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {\n"
       << "  height: 0px;\n"
       << "}\n\n"

       << "QComboBox {\n"
       << "  background: rgba(255, 255, 255, 0.10);\n"
       << "  color: " << COLOR_TEXT_PRIMARY << ";\n"
       << "  border-radius: " << RADIUS_PILL_BUTTON << "px;\n"
       << "  border: 1px solid " << COLOR_BORDER_LIGHT << ";\n"
       << "  padding: 6px 14px;\n"
       << "}\n"
       << "QComboBox::drop-down {\n"
       << "  border: none;\n"
       << "}\n"
       << "QComboBox QAbstractItemView {\n"
       << "  background: #1C1E2C;\n"
       << "  border: 1px solid " << COLOR_BORDER_LIGHT << ";\n"
       << "  selection-background-color: " << COLOR_CORAL_ROSE << ";\n"
       << "  border-radius: 12px;\n"
       << "  padding: 4px;\n"
       << "}\n";

    return ss.str();
}

} // namespace gui
} // namespace aios
