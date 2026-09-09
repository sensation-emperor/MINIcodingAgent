#include "gui/ModelSettingsPanel.h"
#include "providers/ModelRouter.h"
#include "gui/Theme.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>

#ifdef BUILD_GUI
namespace aios {
namespace gui {

// ============================================================================
// ModelSettingsPanel Implementation
// ============================================================================

ModelSettingsPanel::ModelSettingsPanel(QWidget* parent) 
    : QWidget(parent) {
    setupUi();
}

ModelSettingsPanel::~ModelSettingsPanel() = default;

void ModelSettingsPanel::setupUi() {
    setStyleSheet(QString::fromStdString(Theme::getGlobalStyleSheet()));
    
    main_layout_ = new QVBoxLayout(this);
    main_layout_->setContentsMargins(10, 10, 10, 10);
    main_layout_->setSpacing(10);
    
    // Top bar with provider selector and action buttons
    top_bar_layout_ = new QHBoxLayout();
    
    provider_combo_ = new QComboBox();
    provider_combo_->setMinimumWidth(200);
    provider_combo_->setPlaceholderText("Select Provider...");
    connect(provider_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModelSettingsPanel::onProviderSelected);
    
    add_button_ = new QPushButton("+ Add");
    add_button_->setMaximumWidth(80);
    connect(add_button_, &QPushButton::clicked, this, &ModelSettingsPanel::onAddProvider);
    
    remove_button_ = new QPushButton("Remove");
    remove_button_->setMaximumWidth(80);
    connect(remove_button_, &QPushButton::clicked, this, &ModelSettingsPanel::onRemoveProvider);
    
    save_button_ = new QPushButton("Save");
    save_button_->setMaximumWidth(80);
    save_button_->setObjectName("primaryButton");
    connect(save_button_, &QPushButton::clicked, this, &ModelSettingsPanel::onSaveConfig);
    
    test_button_ = new QPushButton("Test");
    test_button_->setMaximumWidth(80);
    connect(test_button_, &QPushButton::clicked, this, &ModelSettingsPanel::onTestConnection);
    
    top_bar_layout_->addWidget(new QLabel("Provider:"));
    top_bar_layout_->addWidget(provider_combo_);
    top_bar_layout_->addStretch();
    top_bar_layout_->addWidget(add_button_);
    top_bar_layout_->addWidget(remove_button_);
    top_bar_layout_->addWidget(test_button_);
    top_bar_layout_->addWidget(save_button_);
    
    main_layout_->addLayout(top_bar_layout_);
    
    // Configuration tabs
    config_tabs_ = new QTabWidget();
    
    // Tab 1: Basic Settings
    basic_tab_ = new QWidget();
    basic_form_ = new QFormLayout(basic_tab_);
    basic_form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    basic_form_->setSpacing(10);
    
    name_edit_ = new QLineEdit();
    name_edit_->setPlaceholderText("e.g., My-LM-Studio");
    basic_form_->addRow("Name:", name_edit_);
    
    type_combo_ = new QComboBox();
    type_combo_->addItem("Local", "local");
    type_combo_->addItem("Cloud", "cloud");
    basic_form_->addRow("Type:", type_combo_);
    
    endpoint_edit_ = new QLineEdit();
    endpoint_edit_->setPlaceholderText("http://localhost:1234/v1");
    basic_form_->addRow("Endpoint:", endpoint_edit_);
    
    api_key_edit_ = new QLineEdit();
    api_key_edit_->setPlaceholderText("sk-...");
    api_key_edit_->setEchoMode(QLineEdit::Password);
    basic_form_->addRow("API Key:", api_key_edit_);
    
    model_edit_ = new QLineEdit();
    model_edit_->setPlaceholderText("e.g., llama-3.2-3b-instruct");
    basic_form_->addRow("Default Model:", model_edit_);
    
    enabled_checkbox_ = new QCheckBox("Enabled");
    enabled_checkbox_->setChecked(true);
    connect(enabled_checkbox_, &QCheckBox::toggled, this, &ModelSettingsPanel::onToggleEnabled);
    basic_form_->addRow("", enabled_checkbox_);
    
    config_tabs_->addTab(basic_tab_, "Basic");
    
    // Tab 2: Advanced Settings
    advanced_tab_ = new QWidget();
    advanced_form_ = new QFormLayout(advanced_tab_);
    advanced_form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    advanced_form_->setSpacing(10);
    
    temperature_spin_ = new QDoubleSpinBox();
    temperature_spin_->setRange(0.0, 2.0);
    temperature_spin_->setSingleStep(0.1);
    temperature_spin_->setValue(0.7);
    temperature_spin_->setDecimals(1);
    advanced_form_->addRow("Temperature:", temperature_spin_);
    
    max_tokens_spin_ = new QSpinBox();
    max_tokens_spin_->setRange(256, 32768);
    max_tokens_spin_->setSingleStep(256);
    max_tokens_spin_->setValue(4096);
    advanced_form_->addRow("Max Tokens:", max_tokens_spin_);
    
    timeout_spin_ = new QSpinBox();
    timeout_spin_->setRange(5, 300);
    timeout_spin_->setSingleStep(5);
    timeout_spin_->setValue(30);
    advanced_form_->addRow("Timeout (sec):", timeout_spin_);
    
    config_tabs_->addTab(advanced_tab_, "Advanced");
    
    // Tab 3: Health & Metrics
    metrics_tab_ = new QWidget();
    metrics_layout_ = new QVBoxLayout(metrics_tab_);
    
    health_status_label_ = new QLabel("Health Status: Unknown");
    health_status_label_->setStyleSheet("font-weight: bold; font-size: 14px;");
    metrics_layout_->addWidget(health_status_label_);
    
    metrics_table_ = new QTableWidget();
    metrics_table_->setColumnCount(2);
    metrics_table_->setHorizontalHeaderLabels({"Metric", "Value"});
    metrics_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    metrics_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    metrics_table_->setRowCount(6);
    
    QStringList metrics = {
        "Total Requests",
        "Success Rate",
        "Avg Latency (ms)",
        "Tokens/sec",
        "Total Tokens",
        "Last Error"
    };
    
    for (int i = 0; i < metrics.size(); ++i) {
        metrics_table_->setItem(i, 0, new QTableWidgetItem(metrics[i]));
        metrics_table_->setItem(i, 1, new QTableWidgetItem("-"));
    }
    
    metrics_layout_->addWidget(metrics_table_);
    
    refresh_metrics_button_ = new QPushButton("Refresh Metrics");
    connect(refresh_metrics_button_, &QPushButton::clicked, this, &ModelSettingsPanel::refreshMetrics);
    metrics_layout_->addWidget(refresh_metrics_button_);
    metrics_layout_->addStretch();
    
    config_tabs_->addTab(metrics_tab_, "Health & Metrics");
    
    main_layout_->addWidget(config_tabs_);
    
    // Initially disable controls until provider is selected
    clearForm();
}

void ModelSettingsPanel::setModelRouter(std::shared_ptr<ModelRouter> router) {
    model_router_ = router;
    refreshProviders();
}

void ModelSettingsPanel::refreshProviders() {
    provider_combo_->clear();
    provider_configs_.clear();
    
    if (!model_router_) return;
    
    auto providers = model_router_->listProviders();
    for (const auto& name : providers) {
        provider_configs_.push_back(ModelProviderConfig{
            .name = name,
            .type = "unknown",
            .endpoint = "",
            .api_key = "",
            .default_model = "",
            .temperature = 0.7,
            .max_tokens = 4096,
            .timeout_seconds = 30,
            .enabled = true,
            .is_active = false
        });
        provider_combo_->addItem(QString::fromStdString(name));
    }
    
    if (!provider_configs_.empty()) {
        loadProviderConfig(provider_configs_[0].name);
    }
}

void ModelSettingsPanel::onProviderSelected(int index) {
    if (index < 0 || index >= static_cast<int>(provider_configs_.size())) {
        clearForm();
        return;
    }
    loadProviderConfig(provider_configs_[index].name);
}

void ModelSettingsPanel::loadProviderConfig(const std::string& name) {
    // In a full implementation, this would load from ModelRouter or config file
    // For now, we'll just populate with placeholder data
    name_edit_->setText(QString::fromStdString(name));
    
    // Try to get health status
    if (model_router_) {
        auto health = model_router_->getProviderHealth(name);
        QString status_text;
        QColor status_color;
        
        switch (health.status) {
            case ProviderStatus::Healthy:
                status_text = "Healthy ✓";
                status_color = Qt::green;
                break;
            case ProviderStatus::Degraded:
                status_text = "Degraded ⚠";
                status_color = Qt::yellow;
                break;
            case ProviderStatus::HalfOpen:
                status_text = "Recovering...";
                status_color = Qt::orange;
                break;
            case ProviderStatus::Offline:
                status_text = "Offline ✗";
                status_color = Qt::red;
                break;
        }
        
        health_status_label_->setText("Health Status: " + status_text);
        health_status_label_->setStyleSheet(
            QString("font-weight: bold; font-size: 14px; color: %1;")
                .arg(status_color.name())
        );
        
        if (!health.last_error.empty()) {
            // Update last error in metrics table
            auto error_item = metrics_table_->item(5, 1);
            if (error_item) {
                error_item->setText(QString::fromStdString(health.last_error));
            }
        }
    }
}

void ModelSettingsPanel::clearForm() {
    name_edit_->clear();
    endpoint_edit_->clear();
    api_key_edit_->clear();
    model_edit_->clear();
    enabled_checkbox_->setChecked(true);
    temperature_spin_->setValue(0.7);
    max_tokens_spin_->setValue(4096);
    timeout_spin_->setValue(30);
    health_status_label_->setText("Health Status: Select a provider");
    
    // Clear metrics table
    for (int i = 0; i < metrics_table_->rowCount(); ++i) {
        auto item = metrics_table_->item(i, 1);
        if (item) item->setText("-");
    }
}

ModelProviderConfig ModelSettingsPanel::getCurrentConfig() const {
    ModelProviderConfig config;
    config.name = name_edit_->text().toStdString();
    config.type = type_combo_->currentData().toString().toStdString();
    config.endpoint = endpoint_edit_->text().toStdString();
    config.api_key = api_key_edit_->text().toStdString();
    config.default_model = model_edit_->text().toStdString();
    config.temperature = temperature_spin_->value();
    config.max_tokens = max_tokens_spin_->value();
    config.timeout_seconds = timeout_spin_->value();
    config.enabled = enabled_checkbox_->isChecked();
    return config;
}

void ModelSettingsPanel::onAddProvider() {
    bool ok = false;
    QString text = QInputDialog::getText(this, "Add Provider",
                                          "Enter provider name:",
                                          QLineEdit::Normal, "", &ok);
    if (ok && !text.isEmpty()) {
        ModelProviderConfig config;
        config.name = text.toStdString();
        config.type = "local";
        config.enabled = true;
        
        provider_configs_.push_back(config);
        provider_combo_->addItem(text);
        provider_combo_->setCurrentText(text);
        
        emit providerAdded(config);
    }
}

void ModelSettingsPanel::onRemoveProvider() {
    int current_index = provider_combo_->currentIndex();
    if (current_index < 0 || provider_configs_.empty()) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Remove Provider",
        "Are you sure you want to remove '" + provider_combo_->currentText() + "'?",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        std::string name = provider_configs_[current_index].name;
        provider_combo_->removeItem(current_index);
        provider_configs_.erase(provider_configs_.begin() + current_index);
        
        emit providerRemoved(name);
        
        if (!provider_configs_.empty()) {
            provider_combo_->setCurrentIndex(0);
            loadProviderConfig(provider_configs_[0].name);
        } else {
            clearForm();
        }
    }
}

void ModelSettingsPanel::onSaveConfig() {
    auto config = getCurrentConfig();
    emit providerConfigChanged(config);
    
    QMessageBox::information(this, "Configuration Saved",
                              "Provider configuration has been saved.");
}

void ModelSettingsPanel::onTestConnection() {
    int current_index = provider_combo_->currentIndex();
    if (current_index < 0 || provider_configs_.empty()) {
        QMessageBox::warning(this, "No Provider Selected",
                              "Please select a provider first.");
        return;
    }
    
    std::string provider_name = provider_configs_[current_index].name;
    emit testConnectionRequested(provider_name);
    
    // Simulate test result (in real implementation, this would be async)
    QMessageBox::information(this, "Connection Test",
                              "Testing connection to " + QString::fromStdString(provider_name) + "...\n\n"
                              "(Implementation pending - would test actual endpoint)");
}

void ModelSettingsPanel::onToggleEnabled(bool checked) {
    // Visual feedback
    if (!checked) {
        name_edit_->setEnabled(false);
        endpoint_edit_->setEnabled(false);
        api_key_edit_->setEnabled(false);
        model_edit_->setEnabled(false);
        temperature_spin_->setEnabled(false);
        max_tokens_spin_->setEnabled(false);
        timeout_spin_->setEnabled(false);
    } else {
        name_edit_->setEnabled(true);
        endpoint_edit_->setEnabled(true);
        api_key_edit_->setEnabled(true);
        model_edit_->setEnabled(true);
        temperature_spin_->setEnabled(true);
        max_tokens_spin_->setEnabled(true);
        timeout_spin_->setEnabled(true);
    }
}

void ModelSettingsPanel::refreshMetrics() {
    if (!model_router_) return;
    
    int current_index = provider_combo_->currentIndex();
    if (current_index < 0 || provider_configs_.empty()) return;
    
    std::string provider_name = provider_configs_[current_index].name;
    auto metrics = model_router_->getMetrics(provider_name);
    
    // Update metrics table
    auto setMetric = [this](int row, const QString& value) {
        auto item = metrics_table_->item(row, 1);
        if (item) item->setText(value);
    };
    
    setMetric(0, QString::number(metrics.total_requests));
    
    double success_rate = 0.0;
    if (metrics.total_requests > 0) {
        success_rate = (static_cast<double>(metrics.successful_requests) / metrics.total_requests) * 100.0;
    }
    setMetric(1, QString::number(success_rate, 'f', 1) + "%");
    
    setMetric(2, QString::number(metrics.average_latency_ms, 'f', 2));
    setMetric(3, QString::number(metrics.average_tokens_per_sec, 'f', 1));
    setMetric(4, QString::number(metrics.total_tokens));
    
    // Last error is already updated via health status
}

// ============================================================================
// SettingsDialog Implementation
// ============================================================================

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setupUi();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUi() {
    setWindowTitle("Settings - UnnatSystems Brahma Coder");
    resize(700, 550);
    setModal(true);
    setStyleSheet(QString::fromStdString(Theme::getGlobalStyleSheet()));
    
    main_layout_ = new QVBoxLayout(this);
    main_layout_->setSpacing(15);
    
    tabs_ = new QTabWidget();
    
    // Tab 1: Model Management
    model_panel_ = new ModelSettingsPanel();
    tabs_->addTab(model_panel_, "Models");
    
    // Tab 2: General Settings
    general_tab_ = new QWidget();
    general_form_ = new QFormLayout(general_tab_);
    general_form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    general_form_->setSpacing(10);
    
    theme_combo_ = new QComboBox();
    theme_combo_->addItem("Coral Rose (Default)", "coral_rose");
    theme_combo_->addItem("Dark Mode", "dark");
    theme_combo_->addItem("Light Mode", "light");
    general_form_->addRow("Theme:", theme_combo_);
    
    require_approval_checkbox_ = new QCheckBox("Require approval before code changes");
    require_approval_checkbox_->setChecked(true);
    general_form_->addRow("", require_approval_checkbox_);
    
    auto_save_checkbox_ = new QCheckBox("Auto-save conversations");
    auto_save_checkbox_->setChecked(true);
    general_form_->addRow("", auto_save_checkbox_);
    
    max_context_files_spin_ = new QSpinBox();
    max_context_files_spin_->setRange(1, 100);
    max_context_files_spin_->setValue(20);
    general_form_->addRow("Max Context Files:", max_context_files_spin_);
    
    tabs_->addTab(general_tab_, "General");
    
    // Tab 3: Keyboard Shortcuts
    shortcuts_tab_ = new QWidget();
    auto* shortcuts_layout = new QVBoxLayout(shortcuts_tab_);
    
    shortcuts_table_ = new QTableWidget();
    shortcuts_table_->setColumnCount(2);
    shortcuts_table_->setHorizontalHeaderLabels({"Action", "Shortcut"});
    shortcuts_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    shortcuts_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    
    QStringList actions = {
        "Submit Prompt", "Cancel Execution", "Switch to Chat",
        "Switch to Task Graph", "Switch to Diffs", "Switch to Terminal",
        "Open Settings", "Clear Console", "Toggle Dark Mode"
    };
    
    QStringList shortcuts = {
        "Ctrl+Enter", "Esc", "Ctrl+1",
        "Ctrl+2", "Ctrl+3", "Ctrl+4",
        "Ctrl+,", "Ctrl+L", "Ctrl+D"
    };
    
    shortcuts_table_->setRowCount(actions.size());
    for (int i = 0; i < actions.size(); ++i) {
        shortcuts_table_->setItem(i, 0, new QTableWidgetItem(actions[i]));
        shortcuts_table_->setItem(i, 1, new QTableWidgetItem(shortcuts[i]));
    }
    
    shortcuts_layout->addWidget(shortcuts_table_);
    tabs_->addTab(shortcuts_tab_, "Shortcuts");
    
    // Tab 4: About
    about_tab_ = new QWidget();
    about_layout_ = new QVBoxLayout(about_tab_);
    about_layout_->setAlignment(Qt::AlignCenter);
    
    logo_label_ = new QLabel("🤖");
    logo_label_->setStyleSheet("font-size: 64px;");
    logo_label_->setAlignment(Qt::AlignCenter);
    about_layout_->addWidget(logo_label_);
    
    version_label_ = new QLabel("<h2>UnnatSystems Brahma Coder</h2>");
    version_label_->setAlignment(Qt::AlignCenter);
    about_layout_->addWidget(version_label_);
    
    description_label_ = new QLabel(
        "<p>Version 1.0.0</p>"
        "<p>Autonomous AI-powered coding agent operating system.</p>"
        "<p>Built with C++23 and Qt 6</p>"
        "<p>© 2026 UnnatSystems</p>"
    );
    description_label_->setAlignment(Qt::AlignCenter);
    description_label_->setWordWrap(true);
    about_layout_->addWidget(description_label_);
    
    tabs_->addTab(about_tab_, "About");
    
    main_layout_->addWidget(tabs_);
    
    // Bottom buttons
    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch();
    
    apply_button_ = new QPushButton("Apply");
    connect(apply_button_, &QPushButton::clicked, this, &SettingsDialog::onApply);
    
    save_button_ = new QPushButton("Save & Close");
    save_button_->setObjectName("primaryButton");
    connect(save_button_, &QPushButton::clicked, this, &SettingsDialog::onSave);
    
    cancel_button_ = new QPushButton("Cancel");
    connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
    
    button_layout->addWidget(apply_button_);
    button_layout->addWidget(save_button_);
    button_layout->addWidget(cancel_button_);
    
    main_layout_->addLayout(button_layout);
}

void SettingsDialog::setModelRouter(std::shared_ptr<ModelRouter> router) {
    model_router_ = router;
    if (model_panel_) {
        model_panel_->setModelRouter(router);
    }
}

void SettingsDialog::loadSettings() {
    // Load settings from config file
    // Placeholder implementation
}

void SettingsDialog::saveSettings() {
    // Save settings to config file
    // Placeholder implementation
    
    // Emit signals for changed settings
    QString theme = theme_combo_->currentData().toString();
    bool require_approval = require_approval_checkbox_->isChecked();
    
    emit themeChanged(theme.toStdString());
    emit permissionsChanged(require_approval);
}

void SettingsDialog::onSave() {
    saveSettings();
    accept();
}

void SettingsDialog::onApply() {
    saveSettings();
}

void SettingsDialog::onReset() {
    // Reset to defaults
    loadSettings();
}

} // namespace gui
} // namespace aios
#endif
