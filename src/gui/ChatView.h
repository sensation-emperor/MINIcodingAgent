#pragma once

#include <string>
#include <vector>

#ifdef BUILD_GUI
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#endif

namespace aios {
namespace gui {

#ifdef BUILD_GUI
class ChatView : public QWidget {
    Q_OBJECT
public:
    explicit ChatView(QWidget* parent = nullptr);
    ~ChatView() override = default;

    void addUserMessage(const QString& text);
    void addAgentMessage(const QString& agent_name, const QString& text, const QString& thought = "");
    void startStreamingMessage(const QString& agent_name);
    void appendStreamingToken(const QString& token);
    void finishStreamingMessage();
    void clear();

private:
    void setupUi();
    void scrollToBottom();

    QVBoxLayout* messages_layout_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QWidget* container_ = nullptr;

    QLabel* current_streaming_label_ = nullptr;
    QString current_streaming_content_;
};
#else
class ChatView {};
#endif

} // namespace gui
} // namespace aios
