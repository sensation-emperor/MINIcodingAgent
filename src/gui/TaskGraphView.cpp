#include "gui/TaskGraphView.h"
#include "gui/Theme.h"
#include <QHBoxLayout>
#include <QFrame>

#ifdef BUILD_GUI
namespace aios {
namespace gui {

TaskGraphView::TaskGraphView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void TaskGraphView::setupUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(16, 8, 16, 8);
    root_layout->setSpacing(12);

    // Summary Card Header
    auto* summary_card = new QWidget(this);
    summary_card->setStyleSheet(QString::fromStdString(Theme::getContainerCardCss()));
    auto* sum_layout = new QVBoxLayout(summary_card);

    goal_label_ = new QLabel("⚡ Active Task Graph: (None)", summary_card);
    goal_label_->setStyleSheet("color: #FF9A56; font-weight: bold; font-size: 15px;");
    sum_layout->addWidget(goal_label_);

    progress_bar_ = new QProgressBar(summary_card);
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setFixedHeight(12);
    progress_bar_->setStyleSheet(
        "QProgressBar { background: rgba(0,0,0,0.3); border-radius: 6px; text-align: center; border: none; }\n"
        "QProgressBar::chunk { background: " + QString::fromStdString(Theme::getBrandGradientCss(false)) + "; border-radius: 6px; }"
    );
    sum_layout->addWidget(progress_bar_);

    progress_label_ = new QLabel("0 / 0 steps completed", summary_card);
    progress_label_->setStyleSheet("color: #A0A5B8; font-size: 12px;");
    sum_layout->addWidget(progress_label_);

    root_layout->addWidget(summary_card);

    // Scrollable Nodes Area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background: transparent;");

    auto* scroll_widget = new QWidget(scroll);
    scroll_widget->setStyleSheet("background: transparent;");
    nodes_layout_ = new QVBoxLayout(scroll_widget);
    nodes_layout_->setContentsMargins(0, 0, 0, 0);
    nodes_layout_->setSpacing(10);
    nodes_layout_->addStretch();

    scroll->setWidget(scroll_widget);
    root_layout->addWidget(scroll, 1);
}

QString TaskGraphView::getStateColor(TaskNodeState state) const {
    switch (state) {
        case TaskNodeState::Completed: return "#2DD4BF"; // Teal
        case TaskNodeState::Running:   return "#FF9A56"; // Orange
        case TaskNodeState::Ready:     return "#38BDF8"; // Sky Blue
        case TaskNodeState::Failed:    return "#F87171"; // Red
        case TaskNodeState::Skipped:   return "#64748B"; // Slate
        case TaskNodeState::Pending:
        default:                       return "#94A3B8"; // Gray
    }
}

QString TaskGraphView::getStateLabel(TaskNodeState state) const {
    switch (state) {
        case TaskNodeState::Completed: return "✓ Completed";
        case TaskNodeState::Running:   return "▶ Running...";
        case TaskNodeState::Ready:     return "Ready";
        case TaskNodeState::Failed:    return "✕ Failed";
        case TaskNodeState::Skipped:   return "⊘ Skipped";
        case TaskNodeState::Pending:
        default:                       return "Pending";
    }
}

QWidget* TaskGraphView::createNodeCard(const TaskNode& node) {
    auto* card = new QWidget();
    card->setStyleSheet(QString::fromStdString(Theme::getContainerCardCss()));

    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(12);

    // Title and description
    auto* text_col = new QVBoxLayout();
    auto* title_lbl = new QLabel(QString::fromStdString(node.id + ": " + node.title), card);
    title_lbl->setStyleSheet("color: #FFFFFF; font-weight: bold; font-size: 13px;");
    text_col->addWidget(title_lbl);

    if (!node.description.empty()) {
        auto* desc_lbl = new QLabel(QString::fromStdString(node.description), card);
        desc_lbl->setStyleSheet("color: #A0A5B8; font-size: 12px;");
        text_col->addWidget(desc_lbl);
    }
    layout->addLayout(text_col, 1);

    // Status Pill
    auto* status_lbl = new QLabel(getStateLabel(node.state), card);
    QString color = getStateColor(node.state);
    status_lbl->setStyleSheet(
        "background-color: rgba(0,0,0,0.35);\n"
        "color: " + color + ";\n"
        "border: 1px solid " + color + ";\n"
        "border-radius: 14px;\n"
        "padding: 4px 12px;\n"
        "font-size: 12px;\n"
        "font-weight: bold;\n"
    );
    layout->addWidget(status_lbl);
    node_state_labels_[node.id] = status_lbl;

    return card;
}

void TaskGraphView::updateGraph(const TaskGraph& graph) {
    clear();
    if (goal_label_) {
        goal_label_->setText("⚡ Active Task Graph: " + QString::fromStdString(graph.getGoal()));
    }

    auto nodes = graph.getAllNodes();
    int completed = 0;
    for (const auto& node : nodes) {
        if (node.state == TaskNodeState::Completed) completed++;
        auto* card = createNodeCard(node);
        nodes_layout_->insertWidget(nodes_layout_->count() - 1, card);
    }

    if (progress_bar_ && !nodes.empty()) {
        int pct = static_cast<int>((static_cast<double>(completed) / nodes.size()) * 100.0);
        progress_bar_->setValue(pct);
        if (progress_label_) {
            progress_label_->setText(QString::number(completed) + " / " + QString::number(nodes.size()) + " steps completed (" + QString::number(pct) + "%)");
        }
    }
}

void TaskGraphView::updateNodeState(const QString& node_id, TaskNodeState state) {
    std::string nid = node_id.toStdString();
    auto it = node_state_labels_.find(nid);
    if (it != node_state_labels_.end() && it->second) {
        QString color = getStateColor(state);
        it->second->setText(getStateLabel(state));
        it->second->setStyleSheet(
            "background-color: rgba(0,0,0,0.35);\n"
            "color: " + color + ";\n"
            "border: 1px solid " + color + ";\n"
            "border-radius: 14px;\n"
            "padding: 4px 12px;\n"
            "font-size: 12px;\n"
            "font-weight: bold;\n"
        );
    }
}

void TaskGraphView::clear() {
    node_state_labels_.clear();
    QLayoutItem* item;
    while ((item = nodes_layout_->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    nodes_layout_->addStretch();
}

} // namespace gui
} // namespace aios
#endif
