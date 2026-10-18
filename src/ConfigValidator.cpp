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

#include "ConfigValidator.hpp"

#include <boost/asio.hpp>

#include <filesystem>

namespace Revres {

namespace {

void add_error(
    std::vector<ConfigDiagnostic>& errors,
    std::string field,
    std::string message
) {
    errors.push_back(ConfigDiagnostic{
        .field_ = std::move(field),
        .message_ = std::move(message)
    });
}

bool is_valid_ip_address(std::string_view host) {
    if (host.empty()) {
        return false;
    }

    boost::system::error_code error;
    const auto address =
        boost::asio::ip::make_address(host, error);

    return !error.failed();
}

}  // namespace

std::vector<ConfigDiagnostic> ConfigValidator::validate(
    const ServerConfig& config
) {
    std::vector<ConfigDiagnostic> errors;

    if (config.host_.empty()) {
        add_error(
            errors,
            "server.host",
            "must not be empty"
        );
    }

    if (!is_valid_ip_address(config.host_)) {
        add_error(
            errors,
            "server.host",
            "must be a valid IP address"
        );
    }

    if (config.port_ < 1 || config.port_ > 65535) {
        add_error(
            errors,
            "server.port",
            "must be between 1 and 65535"
        );
    }

    if (config.timeout_seconds_ <= 0) {
        add_error(
            errors,
            "server.timeout",
            "must be greater than 0"
        );
    }

    if (config.max_connections_ <= 0) {
        add_error(
            errors,
            "server.max_connections",
            "must be greater than 0"
        );
    }

    for (std::size_t index = 0; index < config.routes_.size(); ++index) {
        const auto& route = config.routes_[index];

        const std::string prefix = "routes[" + std::to_string(index) + "]";

        if (route.path_.empty()) {
            add_error(
                errors,
                prefix + ".path",
                "must not be empty"
            );
        } else if (!route.path_.starts_with('/')) {
            add_error(
                errors,
                prefix + ".path",
                "must start with '/'"
            );
        }

        switch (route.type_) {
        case RouteType::Static:
            if (route.root_.empty()) {
                add_error(
                    errors,
                    prefix + ".root",
                    "must not be empty for static routes"
                );
            } else if (!std::filesystem::is_directory(route.root_)) {
                add_error(
                    errors,
                    prefix + ".root",
                    "must refer to an existing directory"
                );
            }
            break;

        case RouteType::Proxy:  // Not implemented yet
            add_error(          // Add a diagnostic for the unimplemented proxy route
                errors,
                prefix + ".type",
                "route type \"proxy\" is not implemented"
            );
            if (route.upstream_.empty()) {
                add_error(
                    errors,
                    prefix + ".upstream",
                    "must not be empty for proxy routes"
                );
            }

            if (!is_valid_ip_address(route.upstream_)) {
                add_error(
                    errors,
                    prefix + ".upstream",
                    "must be a valid IP address"
                );
            }
            break;
        }
    }

    return errors;
}

}  // namespace Revres