#include "ModelLibraryScreen.h"
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>

namespace brahma {

// ============================================================================
// ModelCard Implementation
// ============================================================================

ModelCard::ModelCard(QWidget* parent)
    : QWidget(parent)
    , m_nameLabel(nullptr)
    , m_publisherLabel(nullptr)
    , m_sizeLabel(nullptr)
    , m_quantLabel(nullptr)
    , m_contextLabel(nullptr)
    , m_paramsLabel(nullptr)
    , m_capsContainer(nullptr)
    , m_downloadProgress(nullptr)
    , m_downloadStatusLabel(nullptr)
    , m_actionButton(nullptr)
    , m_favoriteButton(nullptr)
{
    setupUI();
}

void ModelCard::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    
    // Header with name and favorite button
    auto* headerLayout = new QHBoxLayout();
    
    m_nameLabel = new QLabel("Model Name");
    m_nameLabel->setFont(Theme::getInstance()->getFont(Theme::FontType::Heading));
    m_nameLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(
        Theme::getInstance()->getColor(Theme::Color::TextPrimary).name()));
    headerLayout->addWidget(m_nameLabel);
    
    m_favoriteButton = new QPushButton("☆");
    m_favoriteButton->setFixedSize(28, 28);
    m_favoriteButton->setFlat(true);
    m_favoriteButton->setStyleSheet(QString(
        "QPushButton { color: %1; font-size: 18px; border: none; }"
        "QPushButton:hover { color: %2; }"
    ).arg(Theme::getInstance()->getColor(Theme::Color::TextSecondary).name())
       .arg(Theme::getInstance()->getColor(Theme::Color::Accent).name()));
    connect(m_favoriteButton, &QPushButton::clicked, this, [this]() {
        m_isFavorite = !m_isFavorite;
        m_favoriteButton->setText(m_isFavorite ? "★" : "☆");
        emit favoriteToggled(m_modelId, m_isFavorite);
    });
    headerLayout->addWidget(m_favoriteButton);
    
    mainLayout->addLayout(headerLayout);
    
    // Publisher
    m_publisherLabel = new QLabel("Publisher");
    m_publisherLabel->setFont(Theme::getInstance()->getFont(Theme::FontType::Small));
    m_publisherLabel->setStyleSheet(QString("color: %1;").arg(
        Theme::getInstance()->getColor(Theme::Color::TextSecondary).name()));
    mainLayout->addWidget(m_publisherLabel);
    
    // Metadata grid
    auto* metaGrid = new QGridLayout();
    metaGrid->setSpacing(6);
    
    m_sizeLabel = new QLabel("Size: --");
    m_quantLabel = new QLabel("Quant: --");
    m_contextLabel = new QLabel("Context: --");
    m_paramsLabel = new QLabel("Params: --");
    
    QFont smallFont = Theme::getInstance()->getFont(Theme::FontType::Small);
    for (auto* label : {m_sizeLabel, m_quantLabel, m_contextLabel, m_paramsLabel}) {
        label->setFont(smallFont);
        label->setStyleSheet(QString("color: %1;").arg(
            Theme::getInstance()->getColor(Theme::Color::TextSecondary).name()));
    }
    
    metaGrid->addWidget(m_sizeLabel, 0, 0);
    metaGrid->addWidget(m_quantLabel, 0, 1);
    metaGrid->addWidget(m_contextLabel, 1, 0);
    metaGrid->addWidget(m_paramsLabel, 1, 1);
    
    mainLayout->addLayout(metaGrid);
    
    // Capabilities badges
    m_capsContainer = new QFrame();
    m_capsContainer->setFrameShape(QFrame::NoFrame);
    auto* capsLayout = new QHBoxLayout(m_capsContainer);
    capsLayout->setSpacing(4);
    capsLayout->setContentsMargins(0, 4, 0, 0);
    mainLayout->addWidget(m_capsContainer);
    
    // Spacer
    mainLayout->addStretch();
    
    // Download progress
    m_downloadProgress = new QProgressBar();
    m_downloadProgress->setVisible(false);
    m_downloadProgress->setRange(0, 100);
    m_downloadProgress->setValue(0);
    m_downloadProgress->setFormat("%p%");
    m_downloadProgress->setStyleSheet(QString(
        "QProgressBar { border: 1px solid %1; border-radius: 3px; background: %2; }"
        "QProgressBar::chunk { background: %3; }"
    ).arg(Theme::getInstance()->getColor(Theme::Color::Border).name())
       .arg(Theme::getInstance()->getColor(Theme::Color::BackgroundSecondary).name())
       .arg(Theme::getInstance()->getColor(Theme::Color::Accent).name()));
    mainLayout->addWidget(m_downloadProgress);
    
    m_downloadStatusLabel = new QLabel("");
    m_downloadStatusLabel->setFont(smallFont);
    m_downloadStatusLabel->setVisible(false);
    mainLayout->addWidget(m_downloadStatusLabel);
    
    // Action button
    m_actionButton = new QPushButton("Download");
    m_actionButton->setMinimumHeight(36);
    m_actionButton->setCursor(Qt::PointingHandCursor);
    updateStyle();
    mainLayout->addWidget(m_actionButton);
    
    connect(m_actionButton, &QPushButton::clicked, this, [this]() {
        if (m_isInstalled) {
            emit openClicked(m_modelId);
        } else if (m_isDownloading) {
            // Could implement pause/resume here
        } else {
            emit downloadClicked(m_modelId);
        }
    });
    
    // Set card style
    setStyleSheet(QString(
        "ModelCard { "
        "  background: %1; "
        "  border: 1px solid %2; "
        "  border-radius: 8px; "
        "}"
        "ModelCard:hover { "
        "  border: 1px solid %3; "
        "}"
    ).arg(Theme::getInstance()->getColor(Theme::Color::BackgroundSecondary).name())
       .arg(Theme::getInstance()->getColor(Theme::Color::Border).name())
       .arg(Theme::getInstance()->getColor(Theme::Color::Accent).name()));
}

void ModelCard::updateStyle() {
    if (m_isInstalled) {
        m_actionButton->setText("Open");
        m_actionButton->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; border: none; border-radius: 6px; font-weight: bold; }"
            "QPushButton:hover { background: %3; }"
        ).arg(Theme::getInstance()->getColor(Theme::Color::Success).name())
           .arg(Theme::getInstance()->getColor(Theme::Color::TextOnAccent).name())
           .arg(Theme::getInstance()->getColor(Theme::Color::Success).darker(110).name()));
    } else if (m_isDownloading) {
        m_actionButton->setText("Downloading...");
        m_actionButton->setEnabled(false);
        m_actionButton->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; border: none; border-radius: 6px; }"
        ).arg(Theme::getInstance()->getColor(Theme::Color::BackgroundTertiary).name())
           .arg(Theme::getInstance()->getColor(Theme::Color::TextSecondary).name()));
    } else {
        m_actionButton->setText("Download");
        m_actionButton->setEnabled(true);
        m_actionButton->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; border: none; border-radius: 6px; font-weight: bold; }"
            "QPushButton:hover { background: %3; }"
        ).arg(Theme::getInstance()->getColor(Theme::Color::Accent).name())
           .arg(Theme::getInstance()->getColor(Theme::Color::TextOnAccent).name())
           .arg(Theme::getInstance()->getColor(Theme::Color::Accent).darker(110).name()));
    }
}

void ModelCard::setModelName(const QString& name) {
    m_nameLabel->setText(name);
    m_modelId = name; // Default, should be set separately
}

void ModelCard::setPublisher(const QString& publisher) {
    m_publisherLabel->setText(publisher);
}

void ModelCard::setQuantization(const QString& quant) {
    m_quantLabel->setText("Quant: " + quant);
}

void ModelCard::setFileSize(const QString& size) {
    m_sizeLabel->setText("Size: " + size);
}

void ModelCard::setContextLength(int context) {
    m_contextLabel->setText("Context: " + QString::number(context));
}

void ModelCard::setParameterCount(const QString& params) {
    m_paramsLabel->setText("Params: " + params);
}

void ModelCard::setCapabilities(const QStringList& caps) {
    // Clear existing badges
    qDeleteAll(m_capsContainer->findChildren<QLabel*>());
    
    for (const auto& cap : caps) {
        auto* badge = new QLabel(cap);
        badge->setFont(Theme::getInstance()->getFont(Theme::FontType::Small));
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(QString(
            "QLabel { background: %1; color: %2; padding: 2px 6px; border-radius: 4px; }"
        ).arg(Theme::getInstance()->getColor(Theme::Color::Accent).lighter(130).name())
           .arg(Theme::getInstance()->getColor(Theme::Color::TextPrimary).name()));
        m_capsContainer->layout()->addWidget(badge);
    }
}

void ModelCard::setDownloadProgress(double progress) {
    m_downloadProgress->setVisible(true);
    m_downloadProgress->setValue(static_cast<int>(progress * 100));
}

void ModelCard::setDownloadStatus(const QString& status) {
    m_downloadStatusLabel->setVisible(true);
    m_downloadStatusLabel->setText(status);
}

void ModelCard::setIsDownloading(bool downloading) {
    m_isDownloading = downloading;
    updateStyle();
}

void ModelCard::setIsInstalled(bool installed) {
    m_isInstalled = installed;
    updateStyle();
}

void ModelCard::setIsFavorite(bool favorite) {
    m_isFavorite = favorite;
    m_favoriteButton->setText(favorite ? "★" : "☆");
}

// ============================================================================
// DownloadItem Implementation
// ============================================================================

DownloadItem::DownloadItem(QWidget* parent)
    : QWidget(parent)
    , m_nameLabel(nullptr)
    , m_sizeLabel(nullptr)
    , m_progressBar(nullptr)
    , m_speedLabel(nullptr)
    , m_etaLabel(nullptr)
    , m_pauseResumeButton(nullptr)
    , m_cancelButton(nullptr)
{
    setupUI();
}

void DownloadItem::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    layout->setContentsMargins(12, 8, 12, 8);
    
    // Name and size row
    auto* topRow = new QHBoxLayout();
    
    m_nameLabel = new QLabel("Model Name");
    m_nameLabel->setFont(Theme::getInstance()->getFont(Theme::FontType::Body));
    m_nameLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(
        Theme::getInstance()->getColor(Theme::Color::TextPrimary).name()));
    topRow->addWidget(m_nameLabel);
    
    topRow->addStretch();
    
    m_sizeLabel = new QLabel("-- MB");
    m_sizeLabel->setFont(Theme::getInstance()->getFont(Theme::FontType::Small));
    m_sizeLabel->setStyleSheet(QString("color: %1;").arg(
        Theme::getInstance()->getColor(Theme::Color::TextSecondary).name()));
    topRow->addWidget(m_sizeLabel);
    
    layout->addLayout(topRow);
    
    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat("%p%");
    m_progressBar->setFixedHeight(20);
    layout->addWidget(m_progressBar);
    
    // Speed and ETA row
    auto* bottomRow = new QHBoxLayout();
    
    m_speedLabel = new QLabel("-- MB/s");
    m_speedLabel->setFont(Theme::getInstance()->getFont(Theme::FontType::Small));
    m_speedLabel->setStyleSheet(QString("color: %1;").arg(
        Theme::getInstance()->getColor(Theme::Color::TextSecondary).name()));
    bottomRow->addWidget(m_speedLabel);
    
    bottomRow->addStretch();
    
    m_etaLabel = new QLabel("-- remaining");
    m_etaLabel->setFont(Theme::getInstance()->getFont(Theme::FontType::Small));
    m_etaLabel->setStyleSheet(QString("color: %1;").arg(
        Theme::getInstance()->getColor(Theme::Color::TextSecondary).name()));
    bottomRow->addWidget(m_etaLabel);
    
    bottomRow->addSpacing(20);
    
    // Control buttons
    m_pauseResumeButton = new QPushButton("⏸");
    m_pauseResumeButton->setFixedSize(28, 28);
    m_pauseResumeButton->setToolTip("Pause Download");
    m_pauseResumeButton->setFlat(true);
    connect(m_pauseResumeButton, &QPushButton::clicked, this, [this]() {
        m_isPaused = !m_isPaused;
        m_pauseResumeButton->setText(m_isPaused ? "▶" : "⏸");
        m_pauseResumeButton->setToolTip(m_isPaused ? "Resume Download" : "Pause Download");
        if (m_isPaused) {
            emit pauseClicked(m_modelId);
        } else {
            emit resumeClicked(m_modelId);
        }
    });
    bottomRow->addWidget(m_pauseResumeButton);
    
    m_cancelButton = new QPushButton("✕");
    m_cancelButton->setFixedSize(28, 28);
    m_cancelButton->setToolTip("Cancel Download");
    m_cancelButton->setFlat(true);
    m_cancelButton->setStyleSheet(QString(
        "QPushButton { color: %1; }"
        "QPushButton:hover { color: %2; background: %3; }"
    ).arg(Theme::getInstance()->getColor(Theme::Color::Error).name())
       .arg(Theme::getInstance()->getColor(Theme::Color::Error).lighter().name())
       .arg(Theme::getInstance()->getColor(Theme::Color::Error).lighter(150).name()));
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        emit cancelClicked(m_modelId);
    });
    bottomRow->addWidget(m_cancelButton);
    
    layout->addLayout(bottomRow);
    
    // Item background
    setStyleSheet(QString(
        "DownloadItem { "
        "  background: %1; "
        "  border: 1px solid %2; "
        "  border-radius: 6px; "
        "}"
    ).arg(Theme::getInstance()->getColor(Theme::Color::BackgroundSecondary).name())
       .arg(Theme::getInstance()->getColor(Theme::Color::Border).name()));
}

void DownloadItem::setModelName(const QString& name) {
    m_nameLabel->setText(name);
    m_modelId = name;
}

void DownloadItem::setFileSize(const QString& size) {
    m_sizeLabel->setText(size);
}

void DownloadItem::setProgress(double progress) {
    m_progressBar->setValue(static_cast<int>(progress * 100));
}

void DownloadItem::setStatus(const QString& status) {
    // Could add status label if needed
}

void DownloadItem::setSpeed(const QString& speed) {
    m_speedLabel->setText(speed);
}

void DownloadItem::setETA(const QString& eta) {
    m_etaLabel->setText(eta);
}

// ============================================================================
// ModelLibraryScreen Implementation
// ============================================================================

ModelLibraryScreen::ModelLibraryScreen(QWidget* parent)
    : QWidget(parent)
    , m_searchEdit(nullptr)
    , m_formatFilter(nullptr)
    , m_capabilityFilter(nullptr)
    , m_viewModeCombo(nullptr)
    , m_refreshButton(nullptr)
    , m_importButton(nullptr)
    , m_downloadsButton(nullptr)
    , m_viewStack(nullptr)
    , m_gridViewPage(nullptr)
    , m_listViewPage(nullptr)
    , m_gridScroll(nullptr)
    , m_listScroll(nullptr)
    , m_gridLayout(nullptr)
    , m_listLayout(nullptr)
    , m_downloadsPanel(nullptr)
    , m_downloadsLayout(nullptr)
    , m_downloadsCount(nullptr)
    , m_clearCompletedButton(nullptr)
    , m_emptyStateWidget(nullptr)
    , m_emptyStateIcon(nullptr)
    , m_emptyStateText(nullptr)
    , m_refreshTimer(nullptr)
    , m_theme(Theme::getInstance())
{
    setupUI();
    loadFilters();
    
    // Auto-refresh every 30 seconds when visible
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(30000);
    connect(m_refreshTimer, &QTimer::timeout, this, &ModelLibraryScreen::refreshModels);
    m_refreshTimer->start();
}

void ModelLibraryScreen::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Toolbar
    setupToolBar();
    mainLayout->addWidget(m_searchEdit->parentWidget());
    
    // View stack (grid/list)
    setupGridView();
    setupListView();
    
    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_gridViewPage);
    m_viewStack->addWidget(m_listViewPage);
    m_viewStack->setCurrentIndex(0); // Grid view default
    mainLayout->addWidget(m_viewStack, 1);
    
    // Downloads panel (hidden by default)
    setupDownloadsPanel();
}

void ModelLibraryScreen::setupToolBar() {
    auto* toolbar = new QFrame(this);
    toolbar->setFrameShape(QFrame::NoFrame);
    toolbar->setStyleSheet(QString(
        "QFrame { background: %1; border-bottom: 1px solid %2; }"
    ).arg(m_theme->getColor(Theme::Color::BackgroundSecondary).name())
       .arg(m_theme->getColor(Theme::Color::Border).name()));
    
    auto* toolbarLayout = new QVBoxLayout(toolbar);
    toolbarLayout->setSpacing(8);
    toolbarLayout->setContentsMargins(16, 12, 16, 12);
    
    // Search row
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search models (e.g., 'llama', 'mistral', '7B')...");
    m_searchEdit->setFixedHeight(40);
    m_searchEdit->setFont(m_theme->getFont(Theme::FontType::Body));
    m_searchEdit->setStyleSheet(QString(
        "QLineEdit { background: %1; border: 1px solid %2; border-radius: 8px; padding: 0 12px; color: %3; }"
        "QLineEdit:focus { border: 1px solid %4; }"
    ).arg(m_theme->getColor(Theme::Color::BackgroundTertiary).name())
       .arg(m_theme->getColor(Theme::Color::Border).name())
       .arg(m_theme->getColor(Theme::Color::TextPrimary).name())
       .arg(m_theme->getColor(Theme::Color::Accent).name()));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ModelLibraryScreen::onSearchTextChanged);
    toolbarLayout->addWidget(m_searchEdit);
    
    // Filters row
    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);
    
    // Format filter
    filterRow->addWidget(new QLabel("Format:"));
    m_formatFilter = new QComboBox(this);
    m_formatFilter->setFixedHeight(36);
    m_formatFilter->addItem("All Formats", "");
    m_formatFilter->addItem("GGUF", "gguf");
    m_formatFilter->addItem("Safetensors", "safetensors");
    m_formatFilter->addItem("PyTorch", "pytorch");
    connect(m_formatFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        onFormatFilterChanged(m_formatFilter->currentData().toString());
    });
    filterRow->addWidget(m_formatFilter);
    
    // Capability filter
    filterRow->addWidget(new QLabel("Capability:"));
    m_capabilityFilter = new QComboBox(this);
    m_capabilityFilter->setFixedHeight(36);
    m_capabilityFilter->addItem("All", "");
    m_capabilityFilter->addItem("Text Only", "text");
    m_capabilityFilter->addItem("Vision", "vision");
    m_capabilityFilter->addItem("Embeddings", "embeddings");
    m_capabilityFilter->addItem("Tool Use", "tools");
    connect(m_capabilityFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        onCapabilityFilterChanged(m_capabilityFilter->currentData().toString());
    });
    filterRow->addWidget(m_capabilityFilter);
    
    filterRow->addStretch();
    
    // View mode
    m_viewModeCombo = new QComboBox(this);
    m_viewModeCombo->setFixedHeight(36);
    m_viewModeCombo->addItem("▦ Grid", 0);
    m_viewModeCombo->addItem("☰ List", 1);
    connect(m_viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ModelLibraryScreen::onViewModeChanged);
    filterRow->addWidget(m_viewModeCombo);
    
    // Refresh button
    m_refreshButton = new QPushButton("⟳");
    m_refreshButton->setFixedSize(36, 36);
    m_refreshButton->setToolTip("Refresh Models");
    m_refreshButton->setFlat(true);
    connect(m_refreshButton, &QPushButton::clicked, this, &ModelLibraryScreen::refreshModels);
    filterRow->addWidget(m_refreshButton);
    
    // Import button
    m_importButton = new QPushButton("📁 Import");
    m_importButton->setFixedHeight(36);
    connect(m_importButton, &QPushButton::clicked, this, &ModelLibraryScreen::onImportModel);
    filterRow->addWidget(m_importButton);
    
    // Downloads button
    m_downloadsButton = new QPushButton("⬇ Downloads");
    m_downloadsButton->setFixedHeight(36);
    connect(m_downloadsButton, &QPushButton::clicked, this, [this]() {
        // Toggle downloads panel visibility
        m_downloadsPanel->setVisible(!m_downloadsPanel->isVisible());
    });
    filterRow->addWidget(m_downloadsButton);
    
    toolbarLayout->addLayout(filterRow);
}

void ModelLibraryScreen::setupGridView() {
    m_gridViewPage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_gridViewPage);
    layout->setContentsMargins(16, 16, 16, 16);
    
    m_gridScroll = new QScrollArea(m_gridViewPage);
    m_gridScroll->setWidgetResizable(true);
    m_gridScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_gridScroll->setFrameShape(QFrame::NoFrame);
    
    auto* gridContainer = new QWidget(m_gridScroll);
    m_gridLayout = new QGridLayout(gridContainer);
    m_gridLayout->setSpacing(16);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    
    m_gridScroll->setWidget(gridContainer);
    layout->addWidget(m_gridScroll);
    
    // Empty state
    m_emptyStateWidget = new QWidget(m_gridViewPage);
    m_emptyStateWidget->setVisible(false);
    auto* emptyLayout = new QVBoxLayout(m_emptyStateWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    
    m_emptyStateIcon = new QLabel("🔍");
    m_emptyStateIcon->setFont(QFont("Segoe UI Emoji", 48));
    m_emptyStateIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyStateIcon);
    
    m_emptyStateText = new QLabel("No models found\nTry adjusting your search or filters");
    m_emptyStateText->setAlignment(Qt::AlignCenter);
    m_emptyStateText->setFont(m_theme->getFont(Theme::FontType::Body));
    m_emptyStateText->setStyleSheet(QString("color: %1;").arg(
        m_theme->getColor(Theme::Color::TextSecondary).name()));
    emptyLayout->addWidget(m_emptyStateText);
    
    layout->addWidget(m_emptyStateWidget);
}

void ModelLibraryScreen::setupListView() {
    m_listViewPage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_listViewPage);
    layout->setContentsMargins(16, 16, 16, 16);
    
    m_listScroll = new QScrollArea(m_listViewPage);
    m_listScroll->setWidgetResizable(true);
    m_listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listScroll->setFrameShape(QFrame::NoFrame);
    
    auto* listContainer = new QWidget(m_listScroll);
    m_listLayout = new QVBoxLayout(listContainer);
    m_listLayout->setSpacing(8);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->addStretch();
    
    m_listScroll->setWidget(listContainer);
    layout->addWidget(m_listScroll);
}

void ModelLibraryScreen::setupDownloadsPanel() {
    m_downloadsPanel = new QFrame(this);
    m_downloadsPanel->setFixedWidth(400);
    m_downloadsPanel->setVisible(false);
    m_downloadsPanel->setStyleSheet(QString(
        "QFrame { background: %1; border-left: 1px solid %2; }"
    ).arg(m_theme->getColor(Theme::Color::BackgroundSecondary).name())
       .arg(m_theme->getColor(Theme::Color::Border).name()));
    
    m_downloadsLayout = new QVBoxLayout(m_downloadsPanel);
    m_downloadsLayout->setSpacing(12);
    m_downloadsLayout->setContentsMargins(16, 16, 16, 16);
    
    // Header
    auto* headerRow = new QHBoxLayout();
    headerRow->addWidget(new QLabel("<b>Downloads</b>"));
    headerRow->addStretch();
    
    m_downloadsCount = new QLabel("0 active");
    m_downloadsCount->setFont(m_theme->getFont(Theme::FontType::Small));
    headerRow->addWidget(m_downloadsCount);
    
    m_downloadsLayout->addLayout(headerRow);
    
    // Downloads list
    auto* scroll = new QScrollArea(m_downloadsPanel);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    
    auto* container = new QWidget(scroll);
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setSpacing(8);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addStretch();
    
    scroll->setWidget(container);
    m_downloadsLayout->addWidget(scroll, 1);
    
    // Clear completed button
    m_clearCompletedButton = new QPushButton("Clear Completed");
    m_clearCompletedButton->setFixedHeight(36);
    m_downloadsLayout->addWidget(m_clearCompletedButton);
}

void ModelLibraryScreen::loadFilters() {
    // Load saved filter preferences from config
    // TODO: Implement with SettingsManager
}

void ModelLibraryScreen::filterModels() {
    QString query = m_searchEdit->text().toLower();
    QString format = m_formatFilter->currentData().toString();
    QString capability = m_capabilityFilter->currentData().toString();
    
    int visibleCount = 0;
    for (auto* card : m_modelCards) {
        bool matchesQuery = query.isEmpty() || card->objectName().toLower().contains(query);
        // TODO: Add format/capability filtering based on model metadata
        bool matchesFilter = true;
        
        card->setVisible(matchesQuery && matchesFilter);
        if (matchesQuery && matchesFilter) visibleCount++;
    }
    
    m_emptyStateWidget->setVisible(visibleCount == 0);
}

void ModelLibraryScreen::refreshModels() {
    // TODO: Fetch latest model list from Hugging Face / local directory
    // For now, clear and repopulate with sample data
}

void ModelLibraryScreen::setSearchQuery(const QString& query) {
    m_searchEdit->setText(query);
}

void ModelLibraryScreen::setFilterFormat(const QString& format) {
    int index = m_formatFilter->findData(format);
    if (index >= 0) m_formatFilter->setCurrentIndex(index);
}

void ModelLibraryScreen::setFilterCapability(const QString& capability) {
    int index = m_capabilityFilter->findData(capability);
    if (index >= 0) m_capabilityFilter->setCurrentIndex(index);
}

void ModelLibraryScreen::addDownload(const QString& modelId, const QString& name, qint64 size) {
    auto* item = new DownloadItem(m_downloadsPanel);
    item->setModelName(name);
    item->setFileSize(QString::number(size / (1024.0 * 1024.0), 'f', 1) + " MB");
    
    // Insert before the stretch
    auto* container = qobject_cast<QWidget*>(m_downloadsLayout->itemAt(1)->widget());
    if (container) {
        auto* containerLayout = qobject_cast<QVBoxLayout*>(container->layout());
        if (containerLayout) {
            int count = containerLayout->count();
            containerLayout->insertWidget(count - 1, item);
        }
    }
    
    m_downloadItems.append(item);
    m_downloadsCount->setText(QString::number(m_downloadItems.size()) + " active");
}

void ModelLibraryScreen::updateDownloadProgress(const QString& modelId, double progress,
                                                 const QString& speed, const QString& eta) {
    for (auto* item : m_downloadItems) {
        if (item->objectName() == modelId) {
            item->setProgress(progress);
            item->setSpeed(speed + " MB/s");
            item->setETA(eta + " remaining");
            break;
        }
    }
}

void ModelLibraryScreen::downloadComplete(const QString& modelId) {
    // Remove from active downloads
    for (auto* item : m_downloadItems) {
        if (item->objectName() == modelId) {
            item->deleteLater();
            m_downloadItems.removeAll(item);
            break;
        }
    }
    m_downloadsCount->setText(QString::number(m_downloadItems.size()) + " active");
    
    // TODO: Update corresponding ModelCard to show "Installed" state
}

void ModelLibraryScreen::downloadFailed(const QString& modelId, const QString& error) {
    // Show error in download item
    for (auto* item : m_downloadItems) {
        if (item->objectName() == modelId) {
            item->setStatus("Failed: " + error);
            break;
        }
    }
}

void ModelLibraryScreen::onSearchTextChanged(const QString& text) {
    filterModels();
}

void ModelLibraryScreen::onFormatFilterChanged(const QString& format) {
    filterModels();
}

void ModelLibraryScreen::onCapabilityFilterChanged(const QString& cap) {
    filterModels();
}

void ModelLibraryScreen::onViewModeChanged(int index) {
    m_viewStack->setCurrentIndex(index);
}

void ModelLibraryScreen::onOpenDownloadsFolder() {
    QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDesktopServices::openUrl(QUrl::fromLocalFile(downloadPath));
}

void ModelLibraryScreen::onImportModel() {
    QString filePath = QFileDialog::getOpenFileName(this, "Import Model", "",
        "GGUF Files (*.gguf);;All Files (*)");
    if (!filePath.isEmpty()) {
        // TODO: Import model to library
        emit modelDownloadRequested(filePath, "");
    }
}

} // namespace brahma
