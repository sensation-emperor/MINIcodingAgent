#include "gui/FloatingIslandNavBar.h"
#include "gui/Theme.h"

#ifdef BUILD_GUI
namespace aios {
namespace gui {

FloatingIslandNavBar::FloatingIslandNavBar(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void FloatingIslandNavBar::setupUi() {
    setFixedHeight(64);
    setStyleSheet(QString::fromStdString(
        "FloatingIslandNavBar, QWidget#navIslandContainer {\n" + 
        Theme::getFloatingIslandCss() + 
        "}\n"
    ));

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(16, 6, 16, 6);
    layout_->setSpacing(12);

    // App Logo / Title Pill
    QLabel* logoLabel = new QLabel("MINIcodingAgent", this);
    logoLabel->setStyleSheet(
        "color: #FFFFFF; font-weight: bold; font-size: 14px; padding-left: 8px; padding-right: 12px;"
    );
    layout_->addWidget(logoLabel);

    layout_->addStretch();

    // Nav Item Buttons
    auto* chatBtn = createNavButton("Chat", "💬", NavigationTab::Chat);
    auto* taskBtn = createNavButton("TaskGraph", "⚡", NavigationTab::TaskGraph);
    auto* diffBtn = createNavButton("Diffs", "📄", NavigationTab::DiffViewer);
    auto* termBtn = createNavButton("Terminal", "💻", NavigationTab::Terminal);
    auto* setBtn  = createNavButton("Settings", "⚙️", NavigationTab::Settings);

    layout_->addWidget(chatBtn);
    layout_->addWidget(taskBtn);
    layout_->addWidget(diffBtn);
    layout_->addWidget(termBtn);
    layout_->addWidget(setBtn);

    layout_->addStretch();

    updateButtonStyles();
}

QPushButton* FloatingIslandNavBar::createNavButton(const QString& text, const QString& icon_text, NavigationTab tab) {
    QPushButton* btn = new QPushButton(icon_text + " " + text, this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(40);
    
    connect(btn, &QPushButton::clicked, this, [this, tab]() {
        setActiveTab(tab);
    });

    buttons_[tab] = btn;
    return btn;
}

void FloatingIslandNavBar::setActiveTab(NavigationTab tab) {
    if (active_tab_ != tab) {
        active_tab_ = tab;
        updateButtonStyles();
        emit tabChanged(tab);
    }
}

void FloatingIslandNavBar::updateButtonStyles() {
    for (auto& [tab, btn] : buttons_) {
        bool is_active = (tab == active_tab_);
        if (is_active) {
            btn->setStyleSheet(QString::fromStdString(Theme::getPillButtonCss(true)));
        } else {
            btn->setStyleSheet(QString::fromStdString(Theme::getPillButtonCss(false)));
        }
    }
}

} // namespace gui
} // namespace aios
#endif
