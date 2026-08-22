#include "AgentToolParser.h"
#include <regex>
#include <sstream>
#include <algorithm>

namespace aios {

bool AgentToolParser::parseJsonBlock(const std::string& json_str, ParsedToolCall& out) {
    try {
        auto j = nlohmann::json::parse(json_str);
        
        std::string name;
        if (j.contains("tool") && j["tool"].is_string()) {
            name = j["tool"].get<std::string>();
        } else if (j.contains("name") && j["name"].is_string()) {
            name = j["name"].get<std::string>();
        } else if (j.contains("tool_name") && j["tool_name"].is_string()) {
            name = j["tool_name"].get<std::string>();
        } else if (j.contains("action") && j["action"].is_string()) {
            name = j["action"].get<std::string>();
        } else if (j.contains("function") && j["function"].is_object() && j["function"].contains("name")) {
            name = j["function"]["name"].get<std::string>();
        }

        if (name.empty()) {
            return false;
        }

        out.tool_name = name;
        out.raw_call = json_str;
        out.is_valid = true;

        nlohmann::json params_obj;
        if (j.contains("params") && j["params"].is_object()) {
            params_obj = j["params"];
        } else if (j.contains("parameters") && j["parameters"].is_object()) {
            params_obj = j["parameters"];
        } else if (j.contains("arguments")) {
            if (j["arguments"].is_object()) {
                params_obj = j["arguments"];
            } else if (j["arguments"].is_string()) {
                try {
                    params_obj = nlohmann::json::parse(j["arguments"].get<std::string>());
                } catch (...) {
                    out.parameters["args"] = j["arguments"].get<std::string>();
                }
            }
        } else if (j.contains("args") && j["args"].is_object()) {
            params_obj = j["args"];
        } else {
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (it.key() != "tool" && it.key() != "name" && it.key() != "tool_name" && 
                    it.key() != "action" && it.key() != "thought" && it.key() != "type") {
                    params_obj[it.key()] = it.value();
                }
            }
        }

        if (params_obj.is_object()) {
            for (auto it = params_obj.begin(); it != params_obj.end(); ++it) {
                if (it.value().is_string()) {
                    out.parameters[it.key()] = it.value().get<std::string>();
                } else {
                    out.parameters[it.key()] = it.value().dump();
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        out.error = e.what();
        return false;
    }
}

std::vector<ParsedToolCall> AgentToolParser::extractJsonCodeBlocks(const std::string& text) {
    std::vector<ParsedToolCall> results;
    std::regex json_block_regex(R"(```(?:json)?\s*(\{[\s\S]*?\})\s*```)", std::regex::icase);
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), json_block_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string json_str = match[1].str();
        ParsedToolCall call;
        if (parseJsonBlock(json_str, call)) {
            results.push_back(call);
        }
    }
    return results;
}

std::vector<ParsedToolCall> AgentToolParser::extractXmlTagBlocks(const std::string& text) {
    std::vector<ParsedToolCall> results;
    
    // Pattern 1: <tool_call name="..."> ... </tool_call> or <tool_call>...</tool_call>
    std::regex tag_regex(R"(<tool_call(?:\s+name=["']([^"']+)["'])?>([\s\S]*?)</tool_call>)", std::regex::icase);
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), tag_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string attr_name = match[1].str();
        std::string content = match[2].str();
        
        ParsedToolCall call;
        call.raw_call = match[0].str();
        
        // Try parsing inner content as JSON
        if (parseJsonBlock(content, call)) {
            if (!attr_name.empty()) {
                call.tool_name = attr_name;
            }
            results.push_back(call);
            continue;
        }

        // If parseJsonBlock failed but attr_name is present, try parsing content as pure parameter JSON object
        if (!attr_name.empty()) {
            try {
                auto j = nlohmann::json::parse(content);
                if (j.is_object()) {
                    call.tool_name = attr_name;
                    call.is_valid = true;

                    nlohmann::json params_obj;
                    if (j.contains("params") && j["params"].is_object()) {
                        params_obj = j["params"];
                    } else if (j.contains("parameters") && j["parameters"].is_object()) {
                        params_obj = j["parameters"];
                    } else if (j.contains("arguments") && j["arguments"].is_object()) {
                        params_obj = j["arguments"];
                    } else if (j.contains("args") && j["args"].is_object()) {
                        params_obj = j["args"];
                    } else {
                        params_obj = j;
                    }

                    for (auto it = params_obj.begin(); it != params_obj.end(); ++it) {
                        if (it.key() != "tool" && it.key() != "name" && it.key() != "tool_name" &&
                            it.key() != "action" && it.key() != "thought" && it.key() != "type") {
                            if (it.value().is_string()) {
                                call.parameters[it.key()] = it.value().get<std::string>();
                            } else {
                                call.parameters[it.key()] = it.value().dump();
                            }
                        }
                    }
                    results.push_back(call);
                    continue;
                }
            } catch (...) {
                // Not JSON, continue to XML sub-tags parsing
            }
        }

        // Try XML sub-tags like <name>...</name> and <parameters>...</parameters>
        std::regex name_tag(R"(<name>([\s\S]*?)</name>)", std::regex::icase);
        std::smatch name_match;
        if (std::regex_search(content, name_match, name_tag)) {
            call.tool_name = name_match[1].str();
        } else if (!attr_name.empty()) {
            call.tool_name = attr_name;
        }

        if (!call.tool_name.empty()) {
            // Extract parameter tags: <param_name>param_value</param_name>
            std::regex param_tag(R"(<([a-zA-Z0-9_]+)>([\s\S]*?)</\1>)");
            auto p_begin = std::sregex_iterator(content.begin(), content.end(), param_tag);
            auto p_end = std::sregex_iterator();
            for (auto p = p_begin; p != p_end; ++p) {
                std::string k = (*p)[1].str();
                if (k != "name" && k != "tool_call") {
                    call.parameters[k] = (*p)[2].str();
                }
            }
            call.is_valid = true;
            results.push_back(call);
        }
    }
    return results;
}

std::vector<ParsedToolCall> AgentToolParser::extractReActActions(const std::string& text) {
    std::vector<ParsedToolCall> results;
    // Format: Action: tool_name(param1="val1", param2="val2") or Action: tool_name: args
    std::regex react_regex(R"((?:Action|TOOL_CALL):\s*([a-zA-Z0-9_]+)(?:\(([\s\S]*?)\)|:\s*([^\n\r]+)))", std::regex::icase);
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), react_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string name = match[1].str();
        std::string args_paren = match[2].str();
        std::string args_colon = match[3].str();

        ParsedToolCall call;
        call.tool_name = name;
        call.raw_call = match[0].str();
        call.is_valid = true;

        if (!args_paren.empty()) {
            // Parse key="value" or key='value' pairs
            std::regex kv_regex(R"re(([a-zA-Z0-9_]+)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^,\)]+)))re");
            auto kv_begin = std::sregex_iterator(args_paren.begin(), args_paren.end(), kv_regex);
            auto kv_end = std::sregex_iterator();
            bool found_any = false;
            for (auto it = kv_begin; it != kv_end; ++it) {
                found_any = true;
                std::string key = (*it)[1].str();
                std::string val;
                if ((*it)[2].matched) val = (*it)[2].str();
                else if ((*it)[3].matched) val = (*it)[3].str();
                else val = (*it)[4].str();
                call.parameters[key] = val;
            }
            if (!found_any && !args_paren.empty()) {
                call.parameters["args"] = args_paren;
            }
        } else if (!args_colon.empty()) {
            call.parameters["args"] = args_colon;
        }

        results.push_back(call);
    }
    return results;
}

std::vector<ParsedToolCall> AgentToolParser::extractEmbeddedJsonObjects(const std::string& text) {
    std::vector<ParsedToolCall> results;
    // Look for raw JSON object { ... "tool": ... } outside of code blocks
    std::regex raw_json_regex(R"re(\{\s*"(?:tool|tool_name|action|name)"\s*:\s*"[^"]+"\s*,[\s\S]*?\})re");
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), raw_json_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string json_str = match[0].str();
        ParsedToolCall call;
        if (parseJsonBlock(json_str, call)) {
            results.push_back(call);
        }
    }
    return results;
}

std::vector<ParsedToolCall> AgentToolParser::parseToolCalls(const std::string& text) {
    // 1. First priority: Markdown code blocks
    auto calls = extractJsonCodeBlocks(text);
    if (!calls.empty()) return calls;

    // 2. Second priority: XML Tag blocks
    calls = extractXmlTagBlocks(text);
    if (!calls.empty()) return calls;

    // 3. Third priority: ReAct style action invocations
    calls = extractReActActions(text);
    if (!calls.empty()) return calls;

    // 4. Fourth priority: Embedded raw JSON objects
    calls = extractEmbeddedJsonObjects(text);
    if (!calls.empty()) return calls;

    // 5. Final fallback: If entire text is JSON
    ParsedToolCall direct_call;
    if (parseJsonBlock(text, direct_call)) {
        return {direct_call};
    }

    return {};
}

bool AgentToolParser::isTaskComplete(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("task_complete") != std::string::npos ||
        lower.find("task complete") != std::string::npos ||
        lower.find("<task_completed/>") != std::string::npos ||
        lower.find("status: completed") != std::string::npos ||
        lower.find("\"status\": \"completed\"") != std::string::npos) {
        return true;
    }

    // If text contains "DONE" as a standalone keyword or conclusion
    if (text.find("DONE") != std::string::npos && parseToolCalls(text).empty()) {
        return true;
    }

    return false;
}

std::string AgentToolParser::extractThought(const std::string& text) {
    // Strip markdown code fences containing JSON
    std::regex block_regex(R"(```(?:json)?\s*[\s\S]*?```)");
    std::string clean = std::regex_replace(text, block_regex, "");

    // Strip XML tool tags
    std::regex tag_regex(R"(<tool_call[\s\S]*?</tool_call>)");
    clean = std::regex_replace(clean, tag_regex, "");

    // Strip ReAct Action lines
    std::regex react_regex(R"((?:Action|TOOL_CALL):[^\n\r]*)");
    clean = std::regex_replace(clean, react_regex, "");

    // Trim whitespace
    clean.erase(0, clean.find_first_not_of(" \t\n\r"));
    clean.erase(clean.find_last_not_of(" \t\n\r") + 1);

    return clean;
}

std::string AgentToolParser::formatToolDefinitionsPrompt(const std::vector<ToolDefinition>& tools) {
    std::ostringstream ss;
    ss << "Available Tools:\n";
    for (const auto& t : tools) {
        ss << "- " << t.name << ": " << t.description << "\n";
        ss << "  Parameters:\n";
        for (const auto& p : t.parameters) {
            auto desc_it = t.parameter_descriptions.find(p);
            std::string desc = (desc_it != t.parameter_descriptions.end()) ? desc_it->second : "Parameter";
            auto req_it = t.parameter_required.find(p);
            bool req = (req_it != t.parameter_required.end()) ? req_it->second : false;
            ss << "    * " << p << " (" << (req ? "required" : "optional") << "): " << desc << "\n";
        }
    }
    ss << "\nTo call a tool, respond with a JSON block:\n"
       << "```json\n"
       << "{\n"
       << "  \"tool\": \"tool_name\",\n"
       << "  \"params\": {\"param_name\": \"value\"}\n"
       << "}\n"
       << "```\n"
       << "When finished with your assigned subtask, output 'TASK_COMPLETE' with your final summary.\n";
    return ss.str();
}

std::string AgentToolParser::formatToolObservation(const std::string& tool_name, const ToolResult& result) {
    std::ostringstream ss;
    ss << "Tool Result for [" << tool_name << "]:\n";
    if (result.success) {
        ss << "Status: SUCCESS\n";
        if (!result.output.empty()) {
            ss << "Output:\n" << result.output << "\n";
        }
    } else {
        ss << "Status: FAILED\n";
        ss << "Error: " << result.error_message << "\n";
        if (result.exit_code != 0) {
            ss << "Exit Code: " << result.exit_code << "\n";
        }
    }
    return ss.str();
}

} // namespace aios
