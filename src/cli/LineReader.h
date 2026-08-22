#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace aios {

class CommandRegistry;

class LineReader {
public:
    explicit LineReader(const std::string& history_file_path = "");
    ~LineReader();

    std::string readLine(const std::string& prompt);
    std::string readMultiline(const std::string& prompt);

    void setCompletionHandler(std::function<std::vector<std::string>(const std::string&)> handler);
    
    // History
    void addHistory(const std::string& line);
    void loadHistory();
    void saveHistory();
    std::vector<std::string> getHistory(size_t limit = 100) const;
    void clearHistory();

    void setMultiline(bool enable) { multiline_mode_ = enable; }
    bool isMultiline() const { return multiline_mode_; }

    static std::string getDefaultHistoryFilePath();

private:
    std::string history_file_;
    std::vector<std::string> history_;
    size_t max_history_size_ = 10000;
    bool multiline_mode_ = false;
    std::function<std::vector<std::string>(const std::string&)> completion_handler_;

    void setupTerminalRawMode(bool enable);
    std::string readLineRaw(const std::string& prompt);
    std::string handleReverseSearch(const std::string& prompt);
};

} // namespace aios
