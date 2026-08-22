#include "gui/CommandPillWidget.h"
#include "gui/Theme.h"

#ifdef BUILD_GUI
namespace aios {
namespace gui {

CommandPillWidget::CommandPillWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void CommandPillWidget::setupUi() {
    setFixedHeight(68);
    setStyleSheet(QString::fromStdString(
        "CommandPillWidget {\n" + 
        Theme::getFloatingIslandCss() + 
        "}\n"
    ));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(10);

    // Mode Selector Pill
    mode_combo_ = new QComboBox(this);
    mode_combo_->addItem("🤖 Orchestrator", static_cast<int>(ExecutionMode::Orchestrator));
    mode_combo_->addItem("⚡ DAG Planner", static_cast<int>(ExecutionMode::DAGPlanner));
    mode_combo_->addItem("👤 Single Agent", static_cast<int>(ExecutionMode::SingleAgent));
    mode_combo_->setFixedHeight(40);
    layout->addWidget(mode_combo_);

    // Provider Selector Pill
    provider_combo_ = new QComboBox(this);
    provider_combo_->addItem("🏠 LM Studio", "lm_studio");
    provider_combo_->addItem("🦙 Ollama", "ollama");
    provider_combo_->addItem("🌐 OpenAI", "openai");
    provider_combo_->addItem("🧠 Anthropic", "anthropic");
    provider_combo_->addItem("🔀 OpenRouter", "openrouter");
    provider_combo_->setFixedHeight(40);
    layout->addWidget(provider_combo_);

    // Text Input
    input_edit_ = new QLineEdit(this);
    input_edit_->setPlaceholderText("Describe your coding task (e.g. 'Build REST API in C++', 'Fix parser crash')...");
    input_edit_->setFixedHeight(42);
    connect(input_edit_, &QLineEdit::returnPressed, this, [this]() {
        if (!is_running_) {
            QString txt = input_edit_->text().trimmed();
            if (!txt.isEmpty()) {
                emit submitPrompt(txt, getSelectedMode(), getSelectedProvider());
            }
        }
    });
    layout->addWidget(input_edit_, 1);

    // Circular Action CTA Button (Send / Cancel)
    action_btn_ = new QPushButton("➔", this);
    action_btn_->setCursor(Qt::PointingHandCursor);
    action_btn_->setStyleSheet(QString::fromStdString(Theme::getCircularButtonCss(42, true)));
    
    connect(action_btn_, &QPushButton::clicked, this, [this]() {
        if (is_running_) {
            emit cancelExecution();
        } else {
            QString txt = input_edit_->text().trimmed();
            if (!txt.isEmpty()) {
                emit submitPrompt(txt, getSelectedMode(), getSelectedProvider());
            }
        }
    });
    layout->addWidget(action_btn_);
}

QString CommandPillWidget::getPromptText() const {
    return input_edit_ ? input_edit_->text().trimmed() : "";
}

void CommandPillWidget::clearPrompt() {
    if (input_edit_) input_edit_->clear();
}

ExecutionMode CommandPillWidget::getSelectedMode() const {
    if (!mode_combo_) return ExecutionMode::Orchestrator;
    return static_cast<ExecutionMode>(mode_combo_->currentData().toInt());
}

QString CommandPillWidget::getSelectedProvider() const {
    return provider_combo_ ? provider_combo_->currentData().toString() : "lm_studio";
}

void CommandPillWidget::setRunningState(bool is_running) {
    is_running_ = is_running;
    if (action_btn_) {
        if (is_running_) {
            action_btn_->setText("■");
            action_btn_->setToolTip("Cancel Execution");
        } else {
            action_btn_->setText("➔");
            action_btn_->setToolTip("Send Prompt");
        }
    }
}

} // namespace gui
} // namespace aios
#endif
