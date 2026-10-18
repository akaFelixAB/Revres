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

#include "ResponseFactory.hpp"

namespace Revres {

response ResponseFactory::error(
    http::status status,
    unsigned version,
    bool keep_alive,
    std::string_view message
) {
    response res{status, version};
    res.set(http::field::content_type, "text/plain; charset=utf-8");
    res.keep_alive(keep_alive);
    res.body() = message;
    res.prepare_payload();
    return res;
}

response ResponseFactory::bad_request(
    unsigned version,
    bool keep_alive,
    std::string_view message
) {
    return error(http::status::bad_request, version, keep_alive, message);
}

response ResponseFactory::forbidden(
    unsigned version,
    bool keep_alive,
    std::string_view message
) {
    return error(http::status::forbidden, version, keep_alive, message);
}

response ResponseFactory::not_found(
    unsigned version,
    bool keep_alive,
    std::string_view message
) {
    return error(http::status::not_found, version, keep_alive, message);
}

response ResponseFactory::internal_server_error(
    unsigned version,
    bool keep_alive,
    std::string_view message
) {
    return error(http::status::internal_server_error, version, keep_alive, message);
}

}  // namespace Revres