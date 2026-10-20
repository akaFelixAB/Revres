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

#include "ConfigLoader.hpp"
#include "ConfigValidator.hpp"
#include "HttpTypes.hpp"

#include <spdlog/spdlog.h>

namespace Revres {

http::verb ConfigLoader::http_verb_from_string(std::string_view value) const {
    if (value == "GET") {
        return http::verb::get;
    } else if (value == "HEAD") {
        return http::verb::head;
    } else if (value == "POST") {
        return http::verb::post;
    } else if (value == "PUT") {
        return http::verb::put;
    } else if (value == "DELETE") {
        return http::verb::delete_;
    } else {
        spdlog::warn("Unknown HTTP verb: '{}', defaulting to 'GET'", value);
        return http::verb::get;
    }
}

std::string_view ConfigLoader::http_verb_to_string(http::verb verb) const {
    switch (verb) {
    case http::verb::get:
        return "GET";
    case http::verb::head:
        return "HEAD";
    case http::verb::post:
        return "POST";
    case http::verb::put:
        return "PUT";
    case http::verb::delete_:
        return "DELETE";
    default:
        return "UNKNOWN";
    }
}

std::optional<RouteType> ConfigLoader::route_type_from_string(std::string_view value) const {
    if (value == "static") {
        return RouteType::Static;
    } else if (value == "proxy") {
        return RouteType::Proxy;
    } else {
        spdlog::warn("Unknown route type: '{}', defaulting to 'static'", value);
        return std::nullopt;  // Return nullopt for unknown types
    }
}

std::string_view ConfigLoader::route_type_to_string(RouteType type) const {
    switch (type) {
    case RouteType::Static:
        return "static";
    case RouteType::Proxy:
        return "proxy";
    default:
        return "unknown";
    }
}

ServerConfig ConfigLoader::load_file(const std::string& file_path) const {
    ServerConfig config;
    try {
        spdlog::info("Loading configuration from {}", file_path);
        // Parse the TOML file
        auto tbl = toml::parse_file(file_path);

        // Parse the server configuration
        if (auto server_node = tbl["server"].as_table()) {
            config.name_ = server_node->get("name")->value_or("Revres");
            config.host_ = server_node->get("host")->value_or("127.0.0.1");
            config.port_ = static_cast<unsigned int>(server_node->get("port")->value_or(8080));
            config.timeout_seconds_ = static_cast<int>(server_node->get("timeout_seconds")->value_or(30));
            config.max_connections_ = static_cast<int>(server_node->get("max_connections")->value_or(10000));
        }

        // Parse the logging configuration
        if (auto logging_node = tbl["logging"].as_table()) {
            config.logging_.console_output_ = static_cast<bool>(logging_node->get("console_output")->value_or(true));
            config.logging_.file_output_ = static_cast<bool>(logging_node->get("file_output")->value_or(true));
            config.logging_.log_file_ = logging_node->get("log_file")->value_or("logs/serv.log");
            config.logging_.max_file_size_ = static_cast<size_t>(logging_node->get("max_file_size")->value_or(10)) * 1_MB;
            config.logging_.max_files_ = static_cast<int>(logging_node->get("max_files")->value_or(3));
        }

        // Parse the routes configuration
        if (auto routes_node = tbl["routes"].as_array()) {
            config.routes_.clear();
            for (const auto& elem : *routes_node) {
                if (auto route_tbl = elem.as_table()) {
                    std::string type_str = route_tbl->get_as<std::string>("type") 
                        ? (*route_tbl)["type"].ref<std::string>() 
                        : "static";

                    RouteConfig route;
                    route.path_ = (*route_tbl)["path"].value_or("");
                    route.type_ = route_type_from_string(type_str).value_or(RouteType::Static);
                    route.root_ = (*route_tbl)["root"].value_or("./public");
                    route.index_ = (*route_tbl)["index"].value_or("index.html");
                    route.upstream_ = (*route_tbl)["upstream"].value_or("");
                    route.methods_.clear();
                    if (auto methods_array = (*route_tbl)["methods"].as_array()) {
                        for (const auto& method_elem : *methods_array) {
                            if (auto method_str = method_elem.as_string()) {
                                route.methods_.push_back(http_verb_from_string(method_str->get()));
                            }
                        }
                    } else {
                        // If no methods are specified, default to GET
                        route.methods_.push_back(http::verb::get);
                    }

                    if (!route.path_.empty()) {
                        config.routes_.push_back(route);
                    }
                }
            }
        }

        // Validate the configuration
        ConfigValidator validator;
        auto diagnostics = validator.validate(config);

        if (!diagnostics.empty()) {
            throw ConfigException(diagnostics);
        }

        return config;
    } catch (const toml::parse_error& err) {
        throw std::runtime_error(
            "TOML Parse Error: " + std::string(err.description()) + " at line " +
            std::to_string(err.source().begin.line)
        );
    } catch (const std::exception& err) {
        throw std::runtime_error("Config Load Failed: " + std::string(err.what()));
    }
}

}  // namespace Revres