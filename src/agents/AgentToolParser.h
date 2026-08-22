#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "tools/ToolRegistry.h"
#include <nlohmann/json.hpp>

namespace aios {

struct ParsedToolCall {
    std::string tool_name;
    std::unordered_map<std::string, std::string> parameters;
    std::string raw_call;
    bool is_valid = false;
    std::string error;
};

class AgentToolParser {
public:
    /**
     * @brief Parse all tool calls found in the LLM response text.
     * Extracts tool calls from JSON blocks, XML tags, and ReAct patterns.
     */
    static std::vector<ParsedToolCall> parseToolCalls(const std::string& text);

    /**
     * @brief Check if the model response signals that the task is finished.
     */
    static bool isTaskComplete(const std::string& text);

    /**
     * @brief Extracts final thought / explanation excluding the tool call markup.
     */
    static std::string extractThought(const std::string& text);

    /**
     * @brief Format tool definitions into system prompt instructions for small models.
     */
    static std::string formatToolDefinitionsPrompt(const std::vector<ToolDefinition>& tools);

    /**
     * @brief Format tool execution result into an observation string for LLM input.
     */
    static std::string formatToolObservation(const std::string& tool_name, const ToolResult& result);

private:
    static bool parseJsonBlock(const std::string& json_str, ParsedToolCall& out);
    static std::vector<ParsedToolCall> extractJsonCodeBlocks(const std::string& text);
    static std::vector<ParsedToolCall> extractXmlTagBlocks(const std::string& text);
    static std::vector<ParsedToolCall> extractReActActions(const std::string& text);
    static std::vector<ParsedToolCall> extractEmbeddedJsonObjects(const std::string& text);
};

} // namespace aios
