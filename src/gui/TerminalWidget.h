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
class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget* parent = nullptr);
    ~TerminalWidget() override = default;

    void appendOutput(const QString& text);
    void clear();

private:
    void setupUi();

    QPlainTextEdit* term_edit_ = nullptr;
};
#else
class TerminalWidget {};
#endif

} // namespace gui
} // namespace aios
