#include "cli/TerminalRenderer.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace aios {

namespace {

struct RGB {
    int r, g, b;
};

RGB getRGBForRole(ColorRole role) {
    switch (role) {
        case ColorRole::BrandPrimary:   return {255, 107, 157}; // #FF6B9D
        case ColorRole::BrandSecondary: return {255, 154, 86};  // #FF9A56
        case ColorRole::Success:        return {45, 212, 191};  // #2DD4BF
        case ColorRole::Error:          return {248, 113, 113}; // #F87171
        case ColorRole::Warning:        return {251, 191, 36};  // #FBBF24
        case ColorRole::TextPrimary:    return {255, 255, 255}; // #FFFFFF
        case ColorRole::TextMuted:      return {160, 165, 184}; // #A0A5B8
        case ColorRole::BackgroundDark: return {18, 19, 26};    // #12131A
        default:                        return {255, 255, 255};
    }
}

std::string getStyleCode(TextStyle style) {
    switch (style) {
        case TextStyle::Bold:      return "\033[1m";
        case TextStyle::Dim:       return "\033[2m";
        case TextStyle::Italic:    return "\033[3m";
        case TextStyle::Underline: return "\033[4m";
        default:                   return "";
    }
}

RGB parseHex(std::string hex) {
    if (!hex.empty() && hex[0] == '#') {
        hex = hex.substr(1);
    }
    if (hex.length() != 6) {
        return {255, 255, 255};
    }
    int r = 0, g = 0, b = 0;
    try {
        r = std::stoi(hex.substr(0, 2), nullptr, 16);
        g = std::stoi(hex.substr(2, 2), nullptr, 16);
        b = std::stoi(hex.substr(4, 2), nullptr, 16);
    } catch (...) {
        return {255, 255, 255};
    }
    return {r, g, b};
}

size_t visibleLength(const std::string& str) {
    // Strip ANSI escape sequences to compute visual width
    size_t len = 0;
    bool in_esc = false;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\033' && i + 1 < str.length() && str[i + 1] == '[') {
            in_esc = true;
            ++i;
            continue;
        }
        if (in_esc) {
            if ((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= 'a' && str[i] <= 'z')) {
                in_esc = false;
            }
            continue;
        }
        // Basic UTF-8 continuation byte skip
        if ((static_cast<unsigned char>(str[i]) & 0xC0) == 0x80) {
            continue;
        }
        ++len;
    }
    return len;
}

} // namespace

TerminalRenderer& TerminalRenderer::instance() {
    static TerminalRenderer inst;
    return inst;
}

void TerminalRenderer::initialize(bool enable_color) {
    color_enabled_ = enable_color;

#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
            SetConsoleMode(hOut, dwMode);
        }
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hIn, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_PROCESSED_INPUT;
            SetConsoleMode(hIn, dwMode);
        }
    }
#endif
}

std::string TerminalRenderer::colorize(const std::string& text, ColorRole role, TextStyle style) const {
    if (!color_enabled_) {
        return text;
    }
    RGB rgb = getRGBForRole(role);
    std::string style_param = "";
    switch (style) {
        case TextStyle::Bold:      style_param = ";1"; break;
        case TextStyle::Dim:       style_param = ";2"; break;
        case TextStyle::Italic:    style_param = ";3"; break;
        case TextStyle::Underline: style_param = ";4"; break;
        default:                   style_param = ""; break;
    }

    std::ostringstream oss;
    if (role == ColorRole::BackgroundDark) {
        oss << "\033[48;2;" << rgb.r << ";" << rgb.g << ";" << rgb.b << style_param << "m" << text << "\033[0m";
    } else {
        oss << "\033[38;2;" << rgb.r << ";" << rgb.g << ";" << rgb.b << style_param << "m" << text << "\033[0m";
    }
    return oss.str();
}

std::string TerminalRenderer::hexColor(const std::string& text, const std::string& hex_code) const {
    if (!color_enabled_) {
        return text;
    }
    RGB rgb = parseHex(hex_code);
    std::ostringstream oss;
    oss << "\033[38;2;" << rgb.r << ";" << rgb.g << ";" << rgb.b << "m" << text << "\033[0m";
    return oss.str();
}

void TerminalRenderer::printBanner() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string banner_text = R"(
     _  _ ___  ____ ____ _  _ ____ ____ 
     |\/| |__] |__| |  | |_/  |___ [__  
     |  | |__] |  | |__| | \_ |___ ___] 
    )";
    std::cout << colorize(banner_text, ColorRole::BrandPrimary, TextStyle::Bold) << std::endl;
    std::cout << colorize("     MINI Coding Agent - AIOS Full Developer Suite v0.1.0\n", ColorRole::BrandSecondary) << std::endl;
}

void TerminalRenderer::printHeader(const std::string& title) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string line(60, '=');
    std::cout << colorize(line, ColorRole::BrandSecondary) << "\n";
    std::cout << colorize("  " + title, ColorRole::BrandPrimary, TextStyle::Bold) << "\n";
    std::cout << colorize(line, ColorRole::BrandSecondary) << std::endl;
}

std::string TerminalRenderer::formatCard(const std::string& title, const std::string& body, ColorRole border_color) const {
    std::ostringstream oss;
    std::vector<std::string> lines;
    std::istringstream stream(body);
    std::string line;
    size_t max_content_len = visibleLength(title) + 6;

    while (std::getline(stream, line)) {
        lines.push_back(line);
        max_content_len = std::max(max_content_len, visibleLength(line));
    }

    size_t card_width = std::max<size_t>(max_content_len + 4, 60);

    // Top border: ╭─ [ Title ] ──╮
    std::string top_border = "╭─ ";
    if (!title.empty()) {
        top_border += "[ " + title + " ] ";
    }
    size_t top_len = visibleLength(top_border);
    if (top_len < card_width) {
        top_border += std::string(card_width - top_len - 1, '-');
    }
    top_border += "╮";
    oss << colorize(top_border, border_color) << "\n";

    // Body lines
    for (const auto& l : lines) {
        size_t l_len = visibleLength(l);
        size_t padding = (card_width > l_len + 3) ? (card_width - l_len - 3) : 0;
        oss << colorize("│ ", border_color) << l << std::string(padding, ' ') << colorize("│", border_color) << "\n";
    }

    // Bottom border: ╰────────────╯
    std::string bot_border = "╰" + std::string(card_width - 2, '-') + "╯";
    oss << colorize(bot_border, border_color) << "\n";

    return oss.str();
}

void TerminalRenderer::printCard(const std::string& title, const std::string& body, ColorRole border_color) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << formatCard(title, body, border_color);
}

std::string TerminalRenderer::formatTable(const std::vector<std::string>& headers,
                                         const std::vector<std::vector<std::string>>& rows) const {
    if (headers.empty()) return "";

    size_t num_cols = headers.size();
    std::vector<size_t> col_widths(num_cols, 0);

    for (size_t i = 0; i < num_cols; ++i) {
        col_widths[i] = visibleLength(headers[i]);
    }
    for (const auto& row : rows) {
        for (size_t i = 0; i < std::min(num_cols, row.size()); ++i) {
            col_widths[i] = std::max(col_widths[i], visibleLength(row[i]));
        }
    }

    std::ostringstream oss;

    // Top border: ┌───┬───┐
    oss << "┌";
    for (size_t i = 0; i < num_cols; ++i) {
        oss << std::string(col_widths[i] + 2, '-');
        if (i + 1 < num_cols) oss << "┬";
    }
    oss << "┐\n";

    // Header row: │ Col1 │ Col2 │
    oss << "│";
    for (size_t i = 0; i < num_cols; ++i) {
        size_t pad = col_widths[i] - visibleLength(headers[i]);
        oss << " " << colorize(headers[i], ColorRole::BrandPrimary, TextStyle::Bold) << std::string(pad + 1, ' ') << "│";
    }
    oss << "\n";

    // Middle separator: ├───┼───┤
    oss << "├";
    for (size_t i = 0; i < num_cols; ++i) {
        oss << std::string(col_widths[i] + 2, '-');
        if (i + 1 < num_cols) oss << "┼";
    }
    oss << "┤\n";

    // Data rows
    for (const auto& row : rows) {
        oss << "│";
        for (size_t i = 0; i < num_cols; ++i) {
            std::string val = (i < row.size()) ? row[i] : "";
            size_t pad = (col_widths[i] >= visibleLength(val)) ? (col_widths[i] - visibleLength(val)) : 0;
            oss << " " << val << std::string(pad + 1, ' ') << "│";
        }
        oss << "\n";
    }

    // Bottom border: └───┴───┘
    oss << "└";
    for (size_t i = 0; i < num_cols; ++i) {
        oss << std::string(col_widths[i] + 2, '-');
        if (i + 1 < num_cols) oss << "┴";
    }
    oss << "┘\n";

    return oss.str();
}

void TerminalRenderer::printTable(const std::vector<std::string>& headers,
                                 const std::vector<std::vector<std::string>>& rows) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << formatTable(headers, rows);
}

std::string TerminalRenderer::formatDiff(const std::string& diff_content) const {
    std::istringstream stream(diff_content);
    std::string line;
    std::ostringstream oss;

    while (std::getline(stream, line)) {
        if (line.starts_with("+++") || line.starts_with("---")) {
            oss << colorize(line, ColorRole::TextMuted, TextStyle::Bold) << "\n";
        } else if (line.starts_with("+")) {
            oss << colorize(line, ColorRole::Success) << "\n";
        } else if (line.starts_with("-")) {
            oss << colorize(line, ColorRole::Error) << "\n";
        } else if (line.starts_with("@@")) {
            oss << colorize(line, ColorRole::BrandSecondary, TextStyle::Bold) << "\n";
        } else {
            oss << line << "\n";
        }
    }
    return oss.str();
}

void TerminalRenderer::printDiff(const std::string& diff_content) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << formatDiff(diff_content);
}

std::string TerminalRenderer::formatMarkdown(const std::string& markdown) const {
    std::istringstream stream(markdown);
    std::string line;
    std::ostringstream oss;
    bool in_code_fence = false;

    while (std::getline(stream, line)) {
        if (line.starts_with("```")) {
            in_code_fence = !in_code_fence;
            oss << colorize(line, ColorRole::BrandSecondary, TextStyle::Dim) << "\n";
            continue;
        }

        if (in_code_fence) {
            oss << "  " << colorize(line, ColorRole::Success) << "\n";
            continue;
        }

        if (line.starts_with("# ")) {
            oss << colorize(line, ColorRole::BrandPrimary, TextStyle::Bold) << "\n";
        } else if (line.starts_with("## ")) {
            oss << colorize(line, ColorRole::BrandSecondary, TextStyle::Bold) << "\n";
        } else if (line.starts_with("### ")) {
            oss << colorize(line, ColorRole::Warning, TextStyle::Bold) << "\n";
        } else if (line.starts_with("- ") || line.starts_with("* ")) {
            oss << colorize("  • ", ColorRole::BrandPrimary) << line.substr(2) << "\n";
        } else {
            oss << line << "\n";
        }
    }
    return oss.str();
}

void TerminalRenderer::printMarkdown(const std::string& markdown) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << formatMarkdown(markdown);
}

void TerminalRenderer::writeToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!in_stream_) {
        in_stream_ = true;
        stream_start_time_ = std::chrono::system_clock::now();
        stream_token_count_ = 0;
    }

    if (token.find("```") != std::string::npos) {
        in_code_block_ = !in_code_block_;
    }

    ++stream_token_count_;
    if (color_enabled_ && in_code_block_) {
        std::cout << "\033[38;2;45;212;191m" << token << "\033[0m";
    } else {
        std::cout << token;
    }
    std::cout.flush();
}

void TerminalRenderer::endTokenStream() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (in_stream_) {
        std::cout << std::endl;
        auto now = std::chrono::system_clock::now();
        double elapsed_sec = std::chrono::duration<double>(now - stream_start_time_).count();
        if (elapsed_sec > 0.1 && stream_token_count_ > 1) {
            double tok_per_sec = static_cast<double>(stream_token_count_) / elapsed_sec;
            std::ostringstream oss;
            oss << "[" << stream_token_count_ << " tokens in " << std::fixed << std::setprecision(2)
                << elapsed_sec << "s (" << std::fixed << std::setprecision(1) << tok_per_sec << " tok/s)]";
            std::cout << colorize(oss.str(), ColorRole::TextMuted, TextStyle::Dim) << std::endl;
        }
        in_stream_ = false;
        in_code_block_ = false;
        stream_token_count_ = 0;
    }
}

void TerminalRenderer::renderSpinner(const std::string& message, int frame_index) {
    static const std::vector<std::string> frames = {
        "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
    };
    std::lock_guard<std::mutex> lock(mutex_);
    std::string frame = frames[static_cast<size_t>(frame_index) % frames.size()];
    std::cout << "\r" << colorize(frame + " ", ColorRole::BrandPrimary) << message << "   \b\b\b";
    std::cout.flush();
}

void TerminalRenderer::clearLine() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\r\033[2K\r";
    std::cout.flush();
}

bool TerminalRenderer::promptApproval(const std::string& prompt_text) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << colorize("[APPROVAL REQUIRED] ", ColorRole::Warning, TextStyle::Bold)
              << prompt_text << " [y/N]: ";
    std::cout.flush();

    std::string resp;
    if (std::getline(std::cin, resp)) {
        // Trim whitespace
        while (!resp.empty() && (resp.front() == ' ' || resp.front() == '\t')) resp.erase(resp.begin());
        while (!resp.empty() && (resp.back() == ' ' || resp.back() == '\t' || resp.back() == '\r')) resp.pop_back();
        return (resp == "y" || resp == "Y" || resp == "yes" || resp == "YES" || resp == "Yes");
    }
    return false;
}

std::string TerminalRenderer::promptInput(const std::string& prompt_text) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << prompt_text;
    std::cout.flush();
    std::string input;
    std::getline(std::cin, input);
    return input;
}

} // namespace aios
