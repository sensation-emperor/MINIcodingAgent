#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QProgressBar>
#include <QTimer>

#include "Theme.h"

namespace brahma {

/**
 * @brief Model card widget for displaying individual model information
 * 
 * Displays model metadata including name, publisher, quantization, size,
 * and capabilities. Supports selection, download, and context menu actions.
 */
class ModelCard : public QWidget {
    Q_OBJECT
    
public:
    explicit ModelCard(QWidget* parent = nullptr);
    
    // Model data setters
    void setModelName(const QString& name);
    void setPublisher(const QString& publisher);
    void setQuantization(const QString& quant);
    void setFileSize(const QString& size);
    void setContextLength(int context);
    void setParameterCount(const QString& params);
    void setCapabilities(const QStringList& caps);
    void setDownloadProgress(double progress); // 0.0 to 1.0
    void setDownloadStatus(const QString& status);
    void setIsDownloading(bool downloading);
    void setIsInstalled(bool installed);
    void setIsFavorite(bool favorite);
    
signals:
    void downloadClicked(const QString& modelId);
    void deleteClicked(const QString& modelId);
    void openClicked(const QString& modelId);
    void favoriteToggled(const QString& modelId, bool favorite);
    
private:
    void setupUI();
    void updateStyle();
    
    // UI Components
    QLabel* m_nameLabel;
    QLabel* m_publisherLabel;
    QLabel* m_sizeLabel;
    QLabel* m_quantLabel;
    QLabel* m_contextLabel;
    QLabel* m_paramsLabel;
    QFrame* m_capsContainer;
    QProgressBar* m_downloadProgress;
    QLabel* m_downloadStatusLabel;
    QPushButton* m_actionButton;
    QPushButton* m_favoriteButton;
    
    // State
    QString m_modelId;
    bool m_isDownloading = false;
    bool m_isInstalled = false;
    bool m_isFavorite = false;
};

/**
 * @brief Download queue item widget
 * 
 * Shows individual download progress with pause/resume/cancel controls
 */
class DownloadItem : public QWidget {
    Q_OBJECT
    
public:
    explicit DownloadItem(QWidget* parent = nullptr);
    
    void setModelName(const QString& name);
    void setFileSize(const QString& size);
    void setProgress(double progress);
    void setStatus(const QString& status);
    void setSpeed(const QString& speed);
    void setETA(const QString& eta);
    
signals:
    void pauseClicked(const QString& modelId);
    void resumeClicked(const QString& modelId);
    void cancelClicked(const QString& modelId);
    
private:
    void setupUI();
    
    QLabel* m_nameLabel;
    QLabel* m_sizeLabel;
    QProgressBar* m_progressBar;
    QLabel* m_speedLabel;
    QLabel* m_etaLabel;
    QPushButton* m_pauseResumeButton;
    QPushButton* m_cancelButton;
    
    QString m_modelId;
    bool m_isPaused = false;
};

/**
 * @brief Model Library Screen - Browse, search, download, and manage local models
 * 
 * Implements LM Studio Bionic feature 8.5 (Model Library & Catalog):
 * - Model discovery and search
 * - Hugging Face integration
 * - Download queue management
 * - Model metadata display
 * - Hardware compatibility estimation
 * - Favorites and tags
 */
class ModelLibraryScreen : public QWidget {
    Q_OBJECT
    
public:
    explicit ModelLibraryScreen(QWidget* parent = nullptr);
    
public slots:
    void refreshModels();
    void setSearchQuery(const QString& query);
    void setFilterFormat(const QString& format);
    void setFilterCapability(const QString& capability);
    void addDownload(const QString& modelId, const QString& name, qint64 size);
    void updateDownloadProgress(const QString& modelId, double progress, 
                                const QString& speed, const QString& eta);
    void downloadComplete(const QString& modelId);
    void downloadFailed(const QString& modelId, const QString& error);
    
signals:
    void modelSelected(const QString& modelId);
    void modelDownloadRequested(const QString& modelUrl, const QString& savePath);
    void downloadPauseRequested(const QString& modelId);
    void downloadResumeRequested(const QString& modelId);
    void downloadCancelRequested(const QString& modelId);
    void modelDeleteRequested(const QString& modelId);
    void openSettingsRequested();
    
private slots:
    void onSearchTextChanged(const QString& text);
    void onFormatFilterChanged(const QString& format);
    void onCapabilityFilterChanged(const QString& cap);
    void onViewModeChanged(int index);
    void onOpenDownloadsFolder();
    void onImportModel();
    
private:
    void setupUI();
    void setupToolBar();
    void setupGridView();
    void setupListView();
    void setupDownloadsPanel();
    void loadFilters();
    void filterModels();
    
    // Toolbar components
    QLineEdit* m_searchEdit;
    QComboBox* m_formatFilter;
    QComboBox* m_capabilityFilter;
    QComboBox* m_viewModeCombo;
    QPushButton* m_refreshButton;
    QPushButton* m_importButton;
    QPushButton* m_downloadsButton;
    
    // View components
    QStackedWidget* m_viewStack;
    QWidget* m_gridViewPage;
    QWidget* m_listViewPage;
    QScrollArea* m_gridScroll;
    QScrollArea* m_listScroll;
    QGridLayout* m_gridLayout;
    QVBoxLayout* m_listLayout;
    
    // Downloads panel (slides in from right)
    QFrame* m_downloadsPanel;
    QVBoxLayout* m_downloadsLayout;
    QLabel* m_downloadsCount;
    QPushButton* m_clearCompletedButton;
    
    // Empty state
    QWidget* m_emptyStateWidget;
    QLabel* m_emptyStateIcon;
    QLabel* m_emptyStateText;
    
    // State
    QList<ModelCard*> m_modelCards;
    QList<DownloadItem*> m_downloadItems;
    QTimer* m_refreshTimer;
    
    // Theme reference
    Theme* m_theme;
};

} // namespace brahma
