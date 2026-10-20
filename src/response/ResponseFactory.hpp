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

#include <boost/beast/http.hpp>

#include <string_view>

#include "http/HttpTypes.hpp"

namespace Revres {

namespace http = boost::beast::http;

class ResponseFactory final {
public:
    [[nodiscard]] static HttpResponse ok(
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "OK"
    );

    [[nodiscard]] static HttpResponse error(
        http::status status,
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "Internal Server Error"
    );

    [[nodiscard]] static HttpResponse bad_request(
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "400 Bad Request"
    );

    [[nodiscard]] static HttpResponse forbidden(
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "403 Forbidden"
    );

    [[nodiscard]] static HttpResponse not_found(
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "404 Not Found"
    );

    [[nodiscard]] static HttpResponse method_not_allowed(
        unsigned version,
        bool keep_alive = false,
        std::string_view allow_methods = "GET, HEAD",
        std::string_view message = "405 Method Not Allowed"
    );

    [[nodiscard]] static HttpResponse internal_server_error(
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "Internal Server Error"
    );
};

}  // namespace Revres