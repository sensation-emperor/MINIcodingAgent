#include "gui/DiffViewer.h"
#include "gui/Theme.h"
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

#ifdef BUILD_GUI
namespace aios {
namespace gui {

class DiffHighlighter : public QSyntaxHighlighter {
public:
    explicit DiffHighlighter(QTextDocument* parent = nullptr) : QSyntaxHighlighter(parent) {
        addFormat.setBackground(QColor(45, 212, 191, 50));
        addFormat.setForeground(QColor("#2DD4BF"));

        delFormat.setBackground(QColor(248, 113, 113, 50));
        delFormat.setForeground(QColor("#F87171"));

        headerFormat.setForeground(QColor("#FF9A56"));
        headerFormat.setFontWeight(QFont::Bold);
    }

protected:
    void highlightBlock(const QString& text) override {
        if (text.startsWith("+") && !text.startsWith("+++")) {
            setFormat(0, text.length(), addFormat);
        } else if (text.startsWith("-") && !text.startsWith("---")) {
            setFormat(0, text.length(), delFormat);
        } else if (text.startsWith("@@") || text.startsWith("diff") || text.startsWith("index")) {
            setFormat(0, text.length(), headerFormat);
        }
    }

private:
    QTextCharFormat addFormat;
    QTextCharFormat delFormat;
    QTextCharFormat headerFormat;
};

DiffViewer::DiffViewer(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void DiffViewer::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 8, 16, 8);

    auto* card = new QWidget(this);
    card->setStyleSheet(QString::fromStdString(Theme::getContainerCardCss()));
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(14, 12, 14, 12);

    file_label_ = new QLabel("📄 Diff Inspector", card);
    file_label_->setStyleSheet("color: #FF6B9D; font-weight: bold; font-size: 14px; margin-bottom: 6px;");
    card_layout->addWidget(file_label_);

    diff_edit_ = new QPlainTextEdit(card);
    diff_edit_->setReadOnly(true);
    diff_edit_->setFont(QFont("Consolas", 10));
    diff_edit_->setStyleSheet(
        "background-color: #0E1017;\n"
        "color: #E2E8F0;\n"
        "border-radius: 10px;\n"
        "border: 1px solid rgba(255, 255, 255, 0.08);\n"
        "padding: 10px;\n"
    );
    new DiffHighlighter(diff_edit_->document());

    card_layout->addWidget(diff_edit_);
    layout->addWidget(card);
}

void DiffViewer::setDiffContent(const QString& filename, const QString& diff_text) {
    if (file_label_) {
        file_label_->setText("📄 Changes: " + (filename.isEmpty() ? "Workspace Diff" : filename));
    }
    if (diff_edit_) {
        diff_edit_->setPlainText(diff_text);
    }
}

void DiffViewer::clear() {
    if (diff_edit_) diff_edit_->clear();
    if (file_label_) file_label_->setText("📄 Diff Inspector");
}

} // namespace gui
} // namespace aios
#endif
