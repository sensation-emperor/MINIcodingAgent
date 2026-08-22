#include "cli/LineReader.h"
#include "cli/TerminalRenderer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <conio.h>
#define IS_A_TTY(f) _isatty(_fileno(f))
#else
#include <unistd.h>
#include <termios.h>
#define IS_A_TTY(f) isatty(fileno(f))
#endif

namespace aios {

namespace {

std::string trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return str.substr(start, end - start);
}

} // namespace

std::string LineReader::getDefaultHistoryFilePath() {
    const char* env_path = std::getenv("AIOS_HISTORY_FILE");
    if (env_path && *env_path) {
        return std::string(env_path);
    }

#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
    if (!home || !*home) {
        home = std::getenv("HOMEPATH");
    }
    if (home && *home) {
        return std::string(home) + "\\.aios_history";
    }
#else
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.aios_history";
    }
#endif

    return ".aios_history";
}

LineReader::LineReader(const std::string& history_file_path) {
    if (history_file_path.empty()) {
        history_file_ = getDefaultHistoryFilePath();
    } else {
        history_file_ = history_file_path;
    }
    loadHistory();
}

LineReader::~LineReader() {
    saveHistory();
}

void LineReader::loadHistory() {
    history_.clear();
    std::ifstream file(history_file_);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::string trimmed = trim(line);
        if (!trimmed.empty()) {
            if (history_.empty() || history_.back() != line) {
                history_.push_back(line);
            }
        }
    }

    if (history_.size() > max_history_size_) {
        history_.erase(history_.begin(), history_.begin() + (history_.size() - max_history_size_));
    }
}

void LineReader::saveHistory() {
    if (history_file_.empty()) return;
    std::ofstream file(history_file_, std::ios::trunc);
    if (!file.is_open()) return;

    for (const auto& line : history_) {
        file << line << "\n";
    }
}

void LineReader::addHistory(const std::string& line) {
    std::string trimmed = trim(line);
    if (trimmed.empty()) return;

    if (!history_.empty() && history_.back() == line) {
        return; // consecutive deduplication
    }

    history_.push_back(line);
    if (history_.size() > max_history_size_) {
        history_.erase(history_.begin());
    }
}

std::vector<std::string> LineReader::getHistory(size_t limit) const {
    if (history_.empty() || limit == 0) return {};
    size_t count = std::min(limit, history_.size());
    return std::vector<std::string>(history_.end() - count, history_.end());
}

void LineReader::clearHistory() {
    history_.clear();
    saveHistory();
}

void LineReader::setCompletionHandler(std::function<std::vector<std::string>(const std::string&)> handler) {
    completion_handler_ = std::move(handler);
}

void LineReader::setupTerminalRawMode(bool enable) {
#ifdef _WIN32
    // Windows raw console mode is configured on initialize
#else
    static struct termios orig_termios;
    static bool is_raw = false;
    if (enable && !is_raw) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_iflag &= ~(IXON | ICRNL);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        is_raw = true;
    } else if (!enable && is_raw) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        is_raw = false;
    }
#endif
}

std::string LineReader::readMultiline(const std::string& prompt) {
    std::cout << prompt << " [Multiline Mode: end with a '.' on a new line or Ctrl+D]\n";
    std::ostringstream oss;
    std::string line;
    bool first = true;

    while (true) {
        std::cout << (first ? "   ... │ " : "   ... │ ");
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            break;
        }

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (trim(line) == ".") {
            break;
        }

        if (!first) {
            oss << "\n";
        }
        oss << line;
        first = false;
    }

    return oss.str();
}

std::string LineReader::handleReverseSearch(const std::string& prompt) {
    std::string search_query = "";
    std::string matched_entry = "";
    size_t match_idx = history_.size();

    auto update_search = [&]() {
        matched_entry = "";
        if (!search_query.empty()) {
            for (size_t i = history_.size(); i > 0; --i) {
                if (history_[i - 1].find(search_query) != std::string::npos) {
                    matched_entry = history_[i - 1];
                    match_idx = i - 1;
                    break;
                }
            }
        }
        TerminalRenderer::instance().clearLine();
        std::cout << "(reverse-i-search)`" << search_query << "': " << matched_entry;
        std::cout.flush();
    };

    update_search();

#ifdef _WIN32
    while (true) {
        int ch = _getch();
        if (ch == 27 || ch == 3) { // ESC or Ctrl+C
            TerminalRenderer::instance().clearLine();
            std::cout << prompt;
            std::cout.flush();
            return "";
        }
        if (ch == 13 || ch == 10) { // Enter
            TerminalRenderer::instance().clearLine();
            std::cout << prompt << matched_entry << std::endl;
            return matched_entry;
        }
        if (ch == 8) { // Backspace
            if (!search_query.empty()) {
                search_query.pop_back();
                update_search();
            }
        } else if (ch == 18) { // Ctrl+R next match
            if (match_idx > 0 && !search_query.empty()) {
                for (size_t i = match_idx; i > 0; --i) {
                    if (history_[i - 1].find(search_query) != std::string::npos) {
                        matched_entry = history_[i - 1];
                        match_idx = i - 1;
                        break;
                    }
                }
                TerminalRenderer::instance().clearLine();
                std::cout << "(reverse-i-search)`" << search_query << "': " << matched_entry;
                std::cout.flush();
            }
        } else if (ch >= 32 && ch <= 126) {
            search_query += static_cast<char>(ch);
            update_search();
        }
    }
#else
    return "";
#endif
}

std::string LineReader::readLine(const std::string& prompt) {
    if (multiline_mode_) {
        return readMultiline(prompt);
    }

    if (!IS_A_TTY(stdin)) {
        std::cout << prompt;
        std::cout.flush();
        std::string line;
        if (!std::getline(std::cin, line)) {
            return "";
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return line;
    }

    std::string buffer;
    size_t cursor = 0;
    int history_index = static_cast<int>(history_.size());
    std::string temp_buffer = "";

    auto redraw = [&]() {
        TerminalRenderer::instance().clearLine();
        std::cout << prompt << buffer;
        if (cursor < buffer.size()) {
            size_t back = buffer.size() - cursor;
            std::cout << "\033[" << back << "D";
        }
        std::cout.flush();
    };

    std::cout << prompt;
    std::cout.flush();

#ifdef _WIN32
    while (true) {
        int ch = _getch();
        if (ch == 0 || ch == 0xE0) {
            // Extended key code (arrows, delete, home, end)
            int ext = _getch();
            if (ext == 72) { // Up Arrow
                if (history_index > 0) {
                    if (history_index == static_cast<int>(history_.size())) {
                        temp_buffer = buffer;
                    }
                    --history_index;
                    buffer = history_[history_index];
                    cursor = buffer.size();
                    redraw();
                }
            } else if (ext == 80) { // Down Arrow
                if (history_index < static_cast<int>(history_.size())) {
                    ++history_index;
                    if (history_index == static_cast<int>(history_.size())) {
                        buffer = temp_buffer;
                    } else {
                        buffer = history_[history_index];
                    }
                    cursor = buffer.size();
                    redraw();
                }
            } else if (ext == 75) { // Left Arrow
                if (cursor > 0) {
                    --cursor;
                    std::cout << "\033[1D";
                    std::cout.flush();
                }
            } else if (ext == 77) { // Right Arrow
                if (cursor < buffer.size()) {
                    ++cursor;
                    std::cout << "\033[1C";
                    std::cout.flush();
                }
            } else if (ext == 71) { // Home
                cursor = 0;
                redraw();
            } else if (ext == 79) { // End
                cursor = buffer.size();
                redraw();
            } else if (ext == 83) { // Delete
                if (cursor < buffer.size()) {
                    buffer.erase(cursor, 1);
                    redraw();
                }
            }
            continue;
        }

        if (ch == 13 || ch == 10) { // Enter
            std::cout << std::endl;
            // Check for backslash continuation
            if (!buffer.empty() && buffer.back() == '\\') {
                buffer.pop_back();
                std::cout << "   ... │ ";
                std::cout.flush();
                std::string next_part;
                if (std::getline(std::cin, next_part)) {
                    if (!next_part.empty() && next_part.back() == '\r') next_part.pop_back();
                    buffer += "\n" + next_part;
                }
            }
            return buffer;
        }

        if (ch == 8) { // Backspace
            if (cursor > 0) {
                buffer.erase(cursor - 1, 1);
                --cursor;
                redraw();
            }
            continue;
        }

        if (ch == 9) { // Tab Autocompletion
            if (completion_handler_) {
                auto completions = completion_handler_(buffer);
                if (completions.size() == 1) {
                    buffer = completions[0];
                    cursor = buffer.size();
                    redraw();
                } else if (completions.size() > 1) {
                    std::cout << "\n";
                    for (const auto& c : completions) {
                        std::cout << "  " << c;
                    }
                    std::cout << "\n";
                    redraw();
                }
            }
            continue;
        }

        if (ch == 18) { // Ctrl+R Reverse search
            std::string matched = handleReverseSearch(prompt);
            if (!matched.empty()) {
                buffer = matched;
                cursor = buffer.size();
            }
            redraw();
            continue;
        }

        if (ch == 1) { // Ctrl+A (Home)
            cursor = 0;
            redraw();
            continue;
        }

        if (ch == 5) { // Ctrl+E (End)
            cursor = buffer.size();
            redraw();
            continue;
        }

        if (ch == 21) { // Ctrl+U (Clear line)
            buffer.clear();
            cursor = 0;
            redraw();
            continue;
        }

        if (ch == 3) { // Ctrl+C
            std::cout << "^C\n";
            buffer.clear();
            cursor = 0;
            redraw();
            continue;
        }

        if (ch == 4) { // Ctrl+D (EOF on empty buffer)
            if (buffer.empty()) {
                return "/exit";
            }
            continue;
        }

        if (ch >= 32 && ch <= 126) {
            buffer.insert(cursor, 1, static_cast<char>(ch));
            ++cursor;
            redraw();
        }
    }
#else
    // Fallback for POSIX or standard streams
    if (!std::getline(std::cin, buffer)) {
        return "";
    }
    if (!buffer.empty() && buffer.back() == '\r') {
        buffer.pop_back();
    }
    return buffer;
#endif
}

} // namespace aios
