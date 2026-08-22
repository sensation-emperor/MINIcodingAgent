#pragma once

#include <string>
#include <vector>
#include "taskgraph/TaskGraph.h"

#ifdef BUILD_GUI
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#endif

namespace aios {
namespace gui {

#ifdef BUILD_GUI
class TaskGraphView : public QWidget {
    Q_OBJECT
public:
    explicit TaskGraphView(QWidget* parent = nullptr);
    ~TaskGraphView() override = default;

    void updateGraph(const TaskGraph& graph);
    void updateNodeState(const QString& node_id, TaskNodeState state);
    void clear();

private:
    void setupUi();
    QWidget* createNodeCard(const TaskNode& node);
    QString getStateColor(TaskNodeState state) const;
    QString getStateLabel(TaskNodeState state) const;

    QVBoxLayout* nodes_layout_ = nullptr;
    QLabel* goal_label_ = nullptr;
    QLabel* progress_label_ = nullptr;
    QProgressBar* progress_bar_ = nullptr;
    std::unordered_map<std::string, QLabel*> node_state_labels_;
};
#else
class TaskGraphView {};
#endif

} // namespace gui
} // namespace aios
