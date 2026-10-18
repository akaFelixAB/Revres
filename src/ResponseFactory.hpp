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

namespace Revres {

namespace http = boost::beast::http;
using response = http::response<http::string_body>;

class ResponseFactory final {
public:
    [[nodiscard]] static response error(
        http::status status,
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "Internal Server Error"
    );

    [[nodiscard]] static response bad_request(
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "400 Bad Request"
    );

    [[nodiscard]] static response forbidden(
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "403 Forbidden"
    );

    [[nodiscard]] static response not_found(
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "404 Not Found"
    );

    [[nodiscard]] static response internal_server_error(
        unsigned version,
        bool keep_alive = false,
        std::string_view message = "Internal Server Error"
    );
};

}  // namespace Revres