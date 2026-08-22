#include "gui/ChatView.h"
#include "gui/Theme.h"
#include <QScrollBar>

#ifdef BUILD_GUI
namespace aios {
namespace gui {

ChatView::ChatView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ChatView::setupUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(16, 8, 16, 8);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setStyleSheet("background: transparent;");

    container_ = new QWidget(scroll_area_);
    container_->setStyleSheet("background: transparent;");

    messages_layout_ = new QVBoxLayout(container_);
    messages_layout_->setContentsMargins(0, 0, 0, 0);
    messages_layout_->setSpacing(14);
    messages_layout_->addStretch();

    scroll_area_->setWidget(container_);
    root_layout->addWidget(scroll_area_);
}

void ChatView::scrollToBottom() {
    if (scroll_area_ && scroll_area_->verticalScrollBar()) {
        scroll_area_->verticalScrollBar()->setValue(
            scroll_area_->verticalScrollBar()->maximum()
        );
    }
}

void ChatView::addUserMessage(const QString& text) {
    auto* msg_card = new QWidget(container_);
    msg_card->setStyleSheet(QString::fromStdString(
        "background-color: rgba(255, 107, 157, 0.15);\n"
        "border: 1px solid rgba(255, 107, 157, 0.35);\n"
        "border-radius: 18px;\n"
        "padding: 12px 16px;\n"
    ));

    auto* layout = new QVBoxLayout(msg_card);
    layout->setContentsMargins(12, 10, 12, 10);

    auto* sender_lbl = new QLabel("👤 You", msg_card);
    sender_lbl->setStyleSheet("color: #FF9A56; font-weight: bold; font-size: 13px;");
    layout->addWidget(sender_lbl);

    auto* text_lbl = new QLabel(text, msg_card);
    text_lbl->setWordWrap(true);
    text_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text_lbl->setStyleSheet("color: #FFFFFF; font-size: 14px; line-height: 1.4;");
    layout->addWidget(text_lbl);

    messages_layout_->insertWidget(messages_layout_->count() - 1, msg_card);
    scrollToBottom();
}

void ChatView::addAgentMessage(const QString& agent_name, const QString& text, const QString& thought) {
    auto* msg_card = new QWidget(container_);
    msg_card->setStyleSheet(QString::fromStdString(Theme::getContainerCardCss()));

    auto* layout = new QVBoxLayout(msg_card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto* header_lbl = new QLabel("🤖 " + agent_name, msg_card);
    header_lbl->setStyleSheet("color: #FF6B9D; font-weight: bold; font-size: 13px;");
    layout->addWidget(header_lbl);

    if (!thought.isEmpty()) {
        auto* thought_box = new QLabel("💭 Thought:\n" + thought, msg_card);
        thought_box->setWordWrap(true);
        thought_box->setStyleSheet(
            "background-color: rgba(0, 0, 0, 0.25);\n"
            "border-left: 3px solid #FF9A56;\n"
            "border-radius: 8px;\n"
            "padding: 8px 12px;\n"
            "color: #A0A5B8;\n"
            "font-style: italic;\n"
        );
        layout->addWidget(thought_box);
    }

    auto* text_lbl = new QLabel(text, msg_card);
    text_lbl->setWordWrap(true);
    text_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text_lbl->setStyleSheet("color: #FFFFFF; font-size: 14px; line-height: 1.4;");
    layout->addWidget(text_lbl);

    messages_layout_->insertWidget(messages_layout_->count() - 1, msg_card);
    scrollToBottom();
}

void ChatView::startStreamingMessage(const QString& agent_name) {
    current_streaming_content_.clear();

    auto* msg_card = new QWidget(container_);
    msg_card->setStyleSheet(QString::fromStdString(Theme::getContainerCardCss()));

    auto* layout = new QVBoxLayout(msg_card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto* header_lbl = new QLabel("🤖 " + agent_name + " (streaming...)", msg_card);
    header_lbl->setStyleSheet("color: #FF6B9D; font-weight: bold; font-size: 13px;");
    layout->addWidget(header_lbl);

    current_streaming_label_ = new QLabel("", msg_card);
    current_streaming_label_->setWordWrap(true);
    current_streaming_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    current_streaming_label_->setStyleSheet("color: #FFFFFF; font-size: 14px;");
    layout->addWidget(current_streaming_label_);

    messages_layout_->insertWidget(messages_layout_->count() - 1, msg_card);
    scrollToBottom();
}

void ChatView::appendStreamingToken(const QString& token) {
    if (current_streaming_label_) {
        current_streaming_content_ += token;
        current_streaming_label_->setText(current_streaming_content_);
        scrollToBottom();
    }
}

void ChatView::finishStreamingMessage() {
    current_streaming_label_ = nullptr;
    current_streaming_content_.clear();
}

void ChatView::clear() {
    QLayoutItem* item;
    while ((item = messages_layout_->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    messages_layout_->addStretch();
}

} // namespace gui
} // namespace aios
#endif
