#include "gui/TerminalWidget.h"
#include "gui/Theme.h"

#ifdef BUILD_GUI
namespace aios {
namespace gui {

TerminalWidget::TerminalWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void TerminalWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 8, 16, 8);

    auto* card = new QWidget(this);
    card->setStyleSheet(QString::fromStdString(Theme::getContainerCardCss()));
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(14, 12, 14, 12);

    auto* title_lbl = new QLabel("💻 Terminal & Build Output", card);
    title_lbl->setStyleSheet("color: #FF9A56; font-weight: bold; font-size: 14px; margin-bottom: 6px;");
    card_layout->addWidget(title_lbl);

    term_edit_ = new QPlainTextEdit(card);
    term_edit_->setReadOnly(true);
    term_edit_->setFont(QFont("Consolas", 10));
    term_edit_->setStyleSheet(
        "background-color: #0B0C10;\n"
        "color: #6EE7B7;\n"
        "border-radius: 10px;\n"
        "border: 1px solid rgba(255, 255, 255, 0.08);\n"
        "padding: 10px;\n"
    );

    card_layout->addWidget(term_edit_);
    layout->addWidget(card);
}

void TerminalWidget::appendOutput(const QString& text) {
    if (term_edit_) {
        term_edit_->appendPlainText(text);
    }
}

void TerminalWidget::clear() {
    if (term_edit_) term_edit_->clear();
}

} // namespace gui
} // namespace aios
#endif
