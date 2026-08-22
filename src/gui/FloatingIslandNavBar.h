#pragma once

#include <string>
#include <vector>
#include <functional>

#ifdef BUILD_GUI
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#endif

namespace aios {
namespace gui {

enum class NavigationTab {
    Chat,
    TaskGraph,
    DiffViewer,
    Terminal,
    Settings
};

#ifdef BUILD_GUI
class FloatingIslandNavBar : public QWidget {
    Q_OBJECT
public:
    explicit FloatingIslandNavBar(QWidget* parent = nullptr);
    ~FloatingIslandNavBar() override = default;

    void setActiveTab(NavigationTab tab);
    NavigationTab getActiveTab() const { return active_tab_; }

signals:
    void tabChanged(NavigationTab new_tab);

private:
    void setupUi();
    QPushButton* createNavButton(const QString& text, const QString& icon_text, NavigationTab tab);
    void updateButtonStyles();

    NavigationTab active_tab_ = NavigationTab::Chat;
    QHBoxLayout* layout_ = nullptr;
    std::unordered_map<NavigationTab, QPushButton*> buttons_;
};
#else
// Headless stub declaration
class FloatingIslandNavBar {
public:
    void setActiveTab(NavigationTab tab) { active_tab_ = tab; }
    NavigationTab getActiveTab() const { return active_tab_; }
private:
    NavigationTab active_tab_ = NavigationTab::Chat;
};
#endif

} // namespace gui
} // namespace aios
