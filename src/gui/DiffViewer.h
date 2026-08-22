#pragma once

#include <string>

#ifdef BUILD_GUI
#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QLabel>
#endif

namespace aios {
namespace gui {

#ifdef BUILD_GUI
class DiffViewer : public QWidget {
    Q_OBJECT
public:
    explicit DiffViewer(QWidget* parent = nullptr);
    ~DiffViewer() override = default;

    void setDiffContent(const QString& filename, const QString& diff_text);
    void clear();

private:
    void setupUi();

    QLabel* file_label_ = nullptr;
    QPlainTextEdit* diff_edit_ = nullptr;
};
#else
class DiffViewer {};
#endif

} // namespace gui
} // namespace aios
