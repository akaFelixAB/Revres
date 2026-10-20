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

#include <boost/beast.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Revres {

class HttpRequest;

// Forward declaration of HTTP types from Boost.Beast
namespace http = boost::beast::http;

// Type alias for HTTP response with string body
using HttpResponse = http::response<http::string_body>;

// Route handler function
using Handler = std::function<
    http::message_generator(
        const HttpRequest&
    )
>;

// Path parameters extracted from the URL
// Example: For a route "/users/:id", 
// if the request is "/users/123", 
// the path parameter would be {"id", "123"}.
using PathParameter = std::pair<std::string, std::string>;

// Matched route with its handler and extracted path parameters
struct Route {
    Handler handler_;
    // Extracted path parameters
    std::vector<std::string> parameter_names_;
};

// Result of a route match attempt
struct MatchResult {
    std::shared_ptr<const Route> route_;
    std::vector<std::string_view> parameter_values_;
    std::string allowed_methods_;

    [[nodiscard]] std::optional<std::string_view> parameter(
        std::string_view name
    ) const noexcept {
        if (!route_) {
            return std::nullopt;
        }

        for (
            std::size_t index = 0;
            index < route_->parameter_names_.size() &&
            index < parameter_values_.size();
            index++
        ) {
            if (route_->parameter_names_[index] == name) {
                return parameter_values_[index];
            }
        }

        return std::nullopt;
    }
};

// Type alias for route table
using RouteTable = std::map<std::string, Handler>;

}