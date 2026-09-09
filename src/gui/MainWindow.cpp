#include "gui/MainWindow.h"
#include "gui/Theme.h"
#include "agents/Orchestrator.h"
#include "planner/planner.h"
#include "providers/ModelRouter.h"
#include "events/EventBus.h"
#include "logging/Logger.h"
#include <thread>
#include <QMetaObject>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>

#ifdef BUILD_GUI
namespace aios {
namespace gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    createMenuBar();
}

MainWindow::~MainWindow() = default;

void MainWindow::createMenuBar() {
    menu_bar_ = menuBar();
    
    // File Menu
    auto* file_menu = menu_bar_->addMenu("&File");
    
    auto* settings_action = file_menu->addAction("&Settings...");
    settings_action->setShortcut(QKeySequence::Preferences);
    connect(settings_action, &QAction::triggered, this, &MainWindow::openSettings);
    
    file_menu->addSeparator();
    
    auto* exit_action = file_menu->addAction("E&xit");
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QWidget::close);
    
    // View Menu
    auto* view_menu = menu_bar_->addMenu("&View");
    
    auto* chat_action = view_menu->addAction("&Chat");
    chat_action->setShortcut(tr("Ctrl+1"));
    connect(chat_action, &QAction::triggered, [this]() {
        if (nav_bar_) nav_bar_->setActiveTab(NavigationTab::Chat);
    });
    
    auto* taskgraph_action = view_menu->addAction("&Task Graph");
    taskgraph_action->setShortcut(tr("Ctrl+2"));
    connect(taskgraph_action, &QAction::triggered, [this]() {
        if (nav_bar_) nav_bar_->setActiveTab(NavigationTab::TaskGraph);
    });
    
    auto* diff_action = view_menu->addAction("&Diffs");
    diff_action->setShortcut(tr("Ctrl+3"));
    connect(diff_action, &QAction::triggered, [this]() {
        if (nav_bar_) nav_bar_->setActiveTab(NavigationTab::DiffViewer);
    });
    
    auto* terminal_action = view_menu->addAction("&Terminal");
    terminal_action->setShortcut(tr("Ctrl+4"));
    connect(terminal_action, &QAction::triggered, [this]() {
        if (nav_bar_) nav_bar_->setActiveTab(NavigationTab::Terminal);
    });
    
    // Tools Menu
    auto* tools_menu = menu_bar_->addMenu("&Tools");
    
    auto* clear_cache_action = tools_menu->addAction("&Clear Cache");
    connect(clear_cache_action, &QAction::triggered, [this]() {
        chat_view_->addAgentMessage("AIOS", "Cache cleared.");
    });
    
    // Help Menu
    auto* help_menu = menu_bar_->addMenu("&Help");
    
    auto* about_action = help_menu->addAction("&About");
    connect(about_action, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About UnnatSystems Brahma Coder",
            "<h2>UnnatSystems Brahma Coder</h2>"
            "<p>Version 1.0.0</p>"
            "<p>Autonomous AI-powered coding agent operating system.</p>"
            "<p>Built with C++23 and Qt 6</p>"
            "<p>© 2026 UnnatSystems</p>"
        );
    });
}

void MainWindow::openSettings() {
    if (!settings_dialog_) {
        settings_dialog_ = new SettingsDialog(this);
        settings_dialog_->setModelRouter(model_router_);
        
        // Connect settings change signals
        connect(settings_dialog_, &SettingsDialog::themeChanged, [this](const std::string& theme) {
            // Apply theme change
            setStyleSheet(QString::fromStdString(Theme::getGlobalStyleSheet()));
        });
        
        connect(settings_dialog_, &SettingsDialog::permissionsChanged, [this](bool require_approval) {
            // Update permission settings
            Logger::info("Permissions updated: require_approval={}", require_approval);
        });
    }
    
    settings_dialog_->show();
    settings_dialog_->raise();
    settings_dialog_->activateWindow();
}

void MainWindow::setupUi() {
    setWindowTitle("UnnatSystems Brahma Coder");
    resize(1200, 800);
    setStyleSheet(QString::fromStdString(Theme::getGlobalStyleSheet()));

    auto* central_widget = new QWidget(this);
    central_widget->setObjectName("centralWidget");
    auto* main_layout = new QVBoxLayout(central_widget);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // 1. Top Floating Island Navigation Bar (Capsule, 14px margins)
    nav_bar_ = new FloatingIslandNavBar(central_widget);
    connect(nav_bar_, &FloatingIslandNavBar::tabChanged, this, &MainWindow::handleTabChange);
    main_layout->addWidget(nav_bar_);

    // 2. Central Viewport Stack
    stacked_widget_ = new QStackedWidget(central_widget);
    stacked_widget_->setStyleSheet("background: transparent;");

    chat_view_ = new ChatView(stacked_widget_);
    taskgraph_view_ = new TaskGraphView(stacked_widget_);
    diff_viewer_ = new DiffViewer(stacked_widget_);
    terminal_widget_ = new TerminalWidget(stacked_widget_);

    stacked_widget_->addWidget(chat_view_);        // 0: Chat
    stacked_widget_->addWidget(taskgraph_view_);   // 1: TaskGraph
    stacked_widget_->addWidget(diff_viewer_);      // 2: Diffs
    stacked_widget_->addWidget(terminal_widget_);  // 3: Terminal

    main_layout->addWidget(stacked_widget_, 1);

    // 3. Bottom Floating Command Capsule Pill
    command_pill_ = new CommandPillWidget(central_widget);
    connect(command_pill_, &CommandPillWidget::submitPrompt, this, &MainWindow::handlePromptSubmit);
    connect(command_pill_, &CommandPillWidget::cancelExecution, this, &MainWindow::handleCancel);
    main_layout->addWidget(command_pill_);

    setCentralWidget(central_widget);

    // Initial greeting
    chat_view_->addAgentMessage("AIOS", "Welcome to UnnatSystems Brahma Coder! Ready to assist with planning, coding, testing, and debugging. Enter your task below.");
}

void MainWindow::setOrchestrator(std::shared_ptr<MultiAgentOrchestrator> orchestrator) {
    orchestrator_ = orchestrator;
}

void MainWindow::setPlanner(std::shared_ptr<Planner> planner) {
    planner_ = planner;
}

void MainWindow::setModelRouter(std::shared_ptr<ModelRouter> router) {
    model_router_ = router;
}

void MainWindow::setEventBus(std::shared_ptr<EventBus> event_bus) {
    event_bus_ = event_bus;
    wireEventBus();
}

void MainWindow::wireEventBus() {
    if (!event_bus_) return;

    event_bus_->subscribe("orchestrator.step_progress", [this](const std::string& data) {
        QMetaObject::invokeMethod(this, [this, data]() {
            if (terminal_widget_) {
                terminal_widget_->appendOutput(QString::fromStdString(data));
            }
        }, Qt::QueuedConnection);
    });
}

void MainWindow::handleTabChange(NavigationTab tab) {
    if (!stacked_widget_) return;

    switch (tab) {
        case NavigationTab::Chat:
            stacked_widget_->setCurrentIndex(0);
            break;
        case NavigationTab::TaskGraph:
            stacked_widget_->setCurrentIndex(1);
            break;
        case NavigationTab::DiffViewer:
            stacked_widget_->setCurrentIndex(2);
            break;
        case NavigationTab::Terminal:
            stacked_widget_->setCurrentIndex(3);
            break;
        default:
            stacked_widget_->setCurrentIndex(0);
            break;
    }
}

void MainWindow::handlePromptSubmit(const QString& prompt, ExecutionMode mode, const QString& provider) {
    chat_view_->addUserMessage(prompt);
    command_pill_->clearPrompt();
    command_pill_->setRunningState(true);

    std::string task_str = prompt.toStdString();
    std::string prov_str = provider.toStdString();

    // Spawn execution on background thread
    std::thread([this, task_str, mode, prov_str]() {
        if (mode == ExecutionMode::Orchestrator && orchestrator_) {
            QMetaObject::invokeMethod(this, [this]() {
                chat_view_->startStreamingMessage("MultiAgentOrchestrator");
            }, Qt::QueuedConnection);

            WorkflowResult res = orchestrator_->runWorkflow(task_str);

            QMetaObject::invokeMethod(this, [this, res]() {
                chat_view_->finishStreamingMessage();
                chat_view_->addAgentMessage(
                    "Orchestrator", 
                    QString::fromStdString(res.summary), 
                    QString::fromStdString("Completed with review score: " + std::to_string(res.review.overall_score))
                );
                if (taskgraph_view_ && !res.plan.steps.empty()) {
                    TaskGraph g(res.plan.goal);
                    for (const auto& step : res.plan.steps) {
                        TaskNode node;
                        node.id = step.id;
                        node.title = step.title;
                        node.description = step.description;
                        node.dependencies = step.dependencies;
                        node.state = TaskNodeState::Completed;
                        g.addNode(node);
                    }
                    taskgraph_view_->updateGraph(g);
                }
                if (diff_viewer_ && !res.final_diff.empty()) {
                    diff_viewer_->setDiffContent("Patch", QString::fromStdString(res.final_diff));
                }
                command_pill_->setRunningState(false);
            }, Qt::QueuedConnection);
        } else if (mode == ExecutionMode::DAGPlanner && planner_) {
            TaskGraph g = planner_->createPlan(task_str, PlanStrategyType::TreeOfThought);

            QMetaObject::invokeMethod(this, [this, g]() {
                taskgraph_view_->updateGraph(g);
                chat_view_->addAgentMessage(
                    "Planner",
                    "Generated DAG Execution Graph with " + QString::number(g.size()) + " tasks. Check the TaskGraph tab for interactive progress."
                );
                nav_bar_->setActiveTab(NavigationTab::TaskGraph);
                command_pill_->setRunningState(false);
            }, Qt::QueuedConnection);
        } else {
            // Single Agent direct dispatch via ModelRouter
            if (model_router_) {
                QMetaObject::invokeMethod(this, [this]() {
                    chat_view_->startStreamingMessage("AI Agent");
                }, Qt::QueuedConnection);

                std::vector<Message> msgs = {{"user", task_str}};
                auto stream_cb = [this](const std::string& delta) {
                    QString qdelta = QString::fromStdString(delta);
                    QMetaObject::invokeMethod(this, [this, qdelta]() {
                        chat_view_->appendStreamingToken(qdelta);
                    }, Qt::QueuedConnection);
                };

                ModelResponse resp = model_router_->routeWithFallback(prov_str, "", msgs, stream_cb);

                QMetaObject::invokeMethod(this, [this, resp]() {
                    chat_view_->finishStreamingMessage();
                    if (!resp.success) {
                        chat_view_->addAgentMessage("AI Agent", "Error: " + QString::fromStdString(resp.error));
                    }
                    command_pill_->setRunningState(false);
                }, Qt::QueuedConnection);
            } else {
                QMetaObject::invokeMethod(this, [this]() {
                    chat_view_->addAgentMessage("AIOS", "Backend services not initialized.");
                    command_pill_->setRunningState(false);
                }, Qt::QueuedConnection);
            }
        }
    }).detach();
}

void MainWindow::handleCancel() {
    if (orchestrator_) orchestrator_->cancel();
    if (planner_) planner_->stop();
    command_pill_->setRunningState(false);
    chat_view_->addAgentMessage("AIOS", "Execution cancelled by user.");
}

} // namespace gui
} // namespace aios
#endif
