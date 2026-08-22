#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <mutex>
#include <chrono>

namespace aios {

enum class TextStyle {
    Normal,
    Bold,
    Dim,
    Italic,
    Underline
};

enum class ColorRole {
    BrandPrimary,    // Coral Rose #FF6B9D
    BrandSecondary,  // Sunset Orange #FF9A56
    Success,         // Teal #2DD4BF
    Error,           // Coral Red #F87171
    Warning,         // Amber #FBBF24
    TextPrimary,     // White #FFFFFF
    TextMuted,       // Grey #A0A5B8
    BackgroundDark   // Dark #12131A
};

class TerminalRenderer {
public:
    static TerminalRenderer& instance();

    void initialize(bool enable_color = true);
    bool isColorEnabled() const { return color_enabled_; }
    void setColorEnabled(bool enabled) { color_enabled_ = enabled; }

    // Color string formatting
    std::string colorize(const std::string& text, ColorRole role, TextStyle style = TextStyle::Normal) const;
    std::string hexColor(const std::string& text, const std::string& hex_code) const;

    // Component Renderers
    void printBanner() const;
    void printHeader(const std::string& title) const;
    void printCard(const std::string& title, const std::string& body, ColorRole border_color = ColorRole::BrandPrimary) const;
    void printTable(const std::vector<std::string>& headers, 
                    const std::vector<std::vector<std::string>>& rows) const;
    void printDiff(const std::string& diff_content) const;
    void printMarkdown(const std::string& markdown) const;

    // String builder variants (useful for testing & buffering)
    std::string formatCard(const std::string& title, const std::string& body, ColorRole border_color = ColorRole::BrandPrimary) const;
    std::string formatTable(const std::vector<std::string>& headers, 
                            const std::vector<std::vector<std::string>>& rows) const;
    std::string formatDiff(const std::string& diff_content) const;
    std::string formatMarkdown(const std::string& markdown) const;

    // Live Streaming & Progress
    void writeToken(const std::string& token);
    void endTokenStream();
    void renderSpinner(const std::string& message, int frame_index);
    void clearLine();

    // Prompts
    bool promptApproval(const std::string& prompt_text);
    std::string promptInput(const std::string& prompt_text);

private:
    TerminalRenderer() = default;
    bool color_enabled_ = true;
    mutable std::mutex mutex_;
    bool in_stream_ = false;
    bool in_code_block_ = false;
    std::chrono::system_clock::time_point stream_start_time_;
    size_t stream_token_count_ = 0;
};

} // namespace aios
