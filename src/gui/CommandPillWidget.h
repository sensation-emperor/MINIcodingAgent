#pragma once

#include <string>
#include <functional>

#ifdef BUILD_GUI
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#endif

namespace aios {
namespace gui {

enum class ExecutionMode {
    Orchestrator,
    SingleAgent,
    DAGPlanner
};

#ifdef BUILD_GUI
class CommandPillWidget : public QWidget {
    Q_OBJECT
public:
    explicit CommandPillWidget(QWidget* parent = nullptr);
    ~CommandPillWidget() override = default;

    QString getPromptText() const;
    void clearPrompt();
    ExecutionMode getSelectedMode() const;
    QString getSelectedProvider() const;
    void setRunningState(bool is_running);

signals:
    void submitPrompt(const QString& prompt, ExecutionMode mode, const QString& provider);
    void cancelExecution();

private:
    void setupUi();

    QLineEdit* input_edit_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QComboBox* provider_combo_ = nullptr;
    QPushButton* action_btn_ = nullptr;
    bool is_running_ = false;
};
#else
class CommandPillWidget {};
#endif

} // namespace gui
} // namespace aios
