// Copyright 2026 Felix Huang

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "http/HttpTypes.hpp"

#include <toml++/toml.hpp>
#include <unordered_map>

namespace Revres {

// Overload operator"" _MB
constexpr size_t operator""_MB(unsigned long long mb) {
    return mb * 1024ULL * 1024ULL;  // A megabyte is 1024 * 1024 bytes
}

// Route type enumeration
enum class RouteType { Static, Proxy };

// Route configuration
struct RouteConfig {
    std::string path_;
    RouteType type_;        // "static", "proxy", etc.
    std::string root_;      // Static file root directory
    std::string index_;     // Default index file
    std::string upstream_;  // Reverse proxy target address
    std::vector<http::verb> methods_;  // Allowed HTTP methods for this route
};

struct LoggingConfig {
    bool console_output_ = true;    // Enable console logging by default
    bool file_output_ = true;       // Enable file logging by default
    std::string log_file_ = "logs/serv.log";
    size_t max_file_size_ = 10_MB;  // Maximum file size is 10 MB
    int max_files_ = 3;
};

// Server configuration
struct ServerConfig {
    std::string name_ = "Revres";  // Server name
    std::string host_ = "127.0.0.1";
    unsigned int port_ = 8080;
    int timeout_seconds_ = 30;
    int max_connections_ = 10000;
    std::vector<RouteConfig> routes_;
    LoggingConfig logging_;  // Logging configuration
};

class ConfigLoader {
public:
    ConfigLoader() = default;
    ~ConfigLoader() = default;

    [[nodiscard]] ServerConfig load_file(const std::string& file_path) const;

    [[nodiscard]] http::verb http_verb_from_string(std::string_view value) const;
    [[nodiscard]] std::string_view http_verb_to_string(http::verb verb) const;
    
    [[nodiscard]] std::optional<RouteType> route_type_from_string(std::string_view value) const;
    [[nodiscard]] std::string_view route_type_to_string(RouteType type) const;
};  // class ConfigLoader

}  // namespace Revres
