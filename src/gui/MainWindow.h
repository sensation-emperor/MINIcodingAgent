#pragma once

#include <string>
#include <memory>
#include "gui/FloatingIslandNavBar.h"
#include "gui/CommandPillWidget.h"
#include "gui/ChatView.h"
#include "gui/DiffViewer.h"
#include "gui/TaskGraphView.h"
#include "gui/TerminalWidget.h"
#include "gui/ModelSettingsPanel.h"

#ifdef BUILD_GUI
#include <QMainWindow>
#include <QStackedWidget>
#include <QVBoxLayout>
#endif

namespace aios {

class MultiAgentOrchestrator;
class Planner;
class ModelRouter;
class EventBus;

namespace gui {

#ifdef BUILD_GUI
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setOrchestrator(std::shared_ptr<MultiAgentOrchestrator> orchestrator);
    void setPlanner(std::shared_ptr<Planner> planner);
    void setModelRouter(std::shared_ptr<ModelRouter> router);
    void setEventBus(std::shared_ptr<EventBus> event_bus);

public slots:
    void openSettings();

private slots:
    void handleTabChange(NavigationTab tab);
    void handlePromptSubmit(const QString& prompt, ExecutionMode mode, const QString& provider);
    void handleCancel();

private:
    void setupUi();
    void wireEventBus();
    void createMenuBar();

    QMenuBar* menu_bar_ = nullptr;
    FloatingIslandNavBar* nav_bar_ = nullptr;
    QStackedWidget* stacked_widget_ = nullptr;
    CommandPillWidget* command_pill_ = nullptr;

    ChatView* chat_view_ = nullptr;
    TaskGraphView* taskgraph_view_ = nullptr;
    DiffViewer* diff_viewer_ = nullptr;
    TerminalWidget* terminal_widget_ = nullptr;

    std::shared_ptr<MultiAgentOrchestrator> orchestrator_;
    std::shared_ptr<Planner> planner_;
    std::shared_ptr<ModelRouter> model_router_;
    std::shared_ptr<EventBus> event_bus_;
    
    SettingsDialog* settings_dialog_ = nullptr;
};
#else
class MainWindow {};
#endif

} // namespace gui
} // namespace aios
