#pragma once

#ifdef BUILD_GUI
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#endif

#include <string>
#include <vector>
#include <memory>

namespace aios {

class ModelRouter;
struct ProviderHealth;
struct ProviderMetricsLedger;

namespace gui {

#ifdef BUILD_GUI

struct ModelProviderConfig {
    std::string name;
    std::string type; // "local" or "cloud"
    std::string endpoint;
    std::string api_key;
    std::string default_model;
    double temperature = 0.7;
    int max_tokens = 4096;
    int timeout_seconds = 30;
    bool enabled = true;
    bool is_active = false;
};

class ModelSettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit ModelSettingsPanel(QWidget* parent = nullptr);
    ~ModelSettingsPanel() override;

    void setModelRouter(std::shared_ptr<ModelRouter> router);
    void refreshProviders();

signals:
    void providerConfigChanged(const ModelProviderConfig& config);
    void providerAdded(const ModelProviderConfig& config);
    void providerRemoved(const std::string& name);
    void testConnectionRequested(const std::string& provider_name);

private slots:
    void onProviderSelected(int index);
    void onAddProvider();
    void onRemoveProvider();
    void onSaveConfig();
    void onTestConnection();
    void onToggleEnabled(bool checked);
    void refreshMetrics();

private:
    void setupUi();
    void loadProviderConfig(const std::string& name);
    void clearForm();
    ModelProviderConfig getCurrentConfig() const;

    QVBoxLayout* main_layout_ = nullptr;
    QHBoxLayout* top_bar_layout_ = nullptr;
    
    QComboBox* provider_combo_ = nullptr;
    QPushButton* add_button_ = nullptr;
    QPushButton* remove_button_ = nullptr;
    QPushButton* save_button_ = nullptr;
    QPushButton* test_button_ = nullptr;
    
    QTabWidget* config_tabs_ = nullptr;
    
    // Basic Settings Tab
    QWidget* basic_tab_ = nullptr;
    QFormLayout* basic_form_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QComboBox* type_combo_ = nullptr;
    QLineEdit* endpoint_edit_ = nullptr;
    QLineEdit* api_key_edit_ = nullptr;
    QLineEdit* model_edit_ = nullptr;
    QCheckBox* enabled_checkbox_ = nullptr;
    
    // Advanced Settings Tab
    QWidget* advanced_tab_ = nullptr;
    QFormLayout* advanced_form_ = nullptr;
    QDoubleSpinBox* temperature_spin_ = nullptr;
    QSpinBox* max_tokens_spin_ = nullptr;
    QSpinBox* timeout_spin_ = nullptr;
    
    // Health & Metrics Tab
    QWidget* metrics_tab_ = nullptr;
    QVBoxLayout* metrics_layout_ = nullptr;
    QLabel* health_status_label_ = nullptr;
    QTableWidget* metrics_table_ = nullptr;
    QPushButton* refresh_metrics_button_ = nullptr;
    
    std::shared_ptr<ModelRouter> model_router_;
    std::vector<ModelProviderConfig> provider_configs_;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

    void setModelRouter(std::shared_ptr<ModelRouter> router);

signals:
    void settingsChanged();
    void themeChanged(const std::string& theme_name);
    void permissionsChanged(bool require_approval);

private slots:
    void onSave();
    void onReset();
    void onApply();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    QVBoxLayout* main_layout_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    
    // Model Management Tab
    ModelSettingsPanel* model_panel_ = nullptr;
    
    // General Settings Tab
    QWidget* general_tab_ = nullptr;
    QFormLayout* general_form_ = nullptr;
    QComboBox* theme_combo_ = nullptr;
    QCheckBox* require_approval_checkbox_ = nullptr;
    QCheckBox* auto_save_checkbox_ = nullptr;
    QSpinBox* max_context_files_spin_ = nullptr;
    
    // Keyboard Shortcuts Tab
    QWidget* shortcuts_tab_ = nullptr;
    QTableWidget* shortcuts_table_ = nullptr;
    
    // About Tab
    QWidget* about_tab_ = nullptr;
    QVBoxLayout* about_layout_ = nullptr;
    QLabel* logo_label_ = nullptr;
    QLabel* version_label_ = nullptr;
    QLabel* description_label_ = nullptr;
    
    QPushButton* save_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
    QPushButton* apply_button_ = nullptr;
    
    std::shared_ptr<ModelRouter> model_router_;
};

#else

// Stub implementations when GUI is not built
class ModelSettingsPanel {};
class SettingsDialog {};

#endif

} // namespace gui
} // namespace aios
