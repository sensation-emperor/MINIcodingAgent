#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <memory>

namespace aios {

struct HttpRequest {
    std::string url;
    std::string method = "GET"; // "GET", "POST", "PUT", "DELETE"
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::chrono::milliseconds timeout{30000};
};

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::string error;
    bool success = false;
    std::chrono::milliseconds elapsed_time{0};
};

using StreamCallback = std::function<void(const std::string& chunk)>;

class HttpClient {
public:
    HttpClient() = default;
    virtual ~HttpClient() = default;

    /**
     * @brief Perform synchronous HTTP request
     */
    virtual HttpResponse request(const HttpRequest& req);

    /**
     * @brief Perform synchronous POST request with JSON payload
     */
    HttpResponse postJson(const std::string& url, 
                         const std::string& json_body, 
                         const std::unordered_map<std::string, std::string>& headers = {},
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));

    /**
     * @brief Perform synchronous GET request
     */
    HttpResponse get(const std::string& url, 
                    const std::unordered_map<std::string, std::string>& headers = {},
                    std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));

    /**
     * @brief Perform streaming POST request (SSE / text/event-stream / ndjson)
     */
    virtual HttpResponse postStream(const std::string& url,
                                   const std::string& json_body,
                                   StreamCallback on_chunk,
                                   const std::unordered_map<std::string, std::string>& headers = {},
                                   std::chrono::milliseconds timeout = std::chrono::milliseconds(60000));

    /**
     * @brief Parse URL into host, port, path, and scheme
     */
    struct ParsedUrl {
        std::string scheme = "http";
        std::string host = "localhost";
        int port = 80;
        std::string path = "/";
    };
    static ParsedUrl parseUrl(const std::string& url);

    /**
     * @brief Process SSE line buffer and extract event data payload
     */
    static void processSseBuffer(const std::string& chunk, 
                                std::string& carry_over, 
                                std::function<void(const std::string& data)> on_data);
};

} // namespace aios
