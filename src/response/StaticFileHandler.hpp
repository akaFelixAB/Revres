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
// StaticFileHandler.hpp

#pragma once

#include "http/HttpTypes.hpp"

#include <filesystem>
#include <string>

namespace Revres {

class HttpRequest;

class StaticFileHandler final {
public:
    StaticFileHandler(
        const std::filesystem::path& root,
        const std::string& index_file = "index.html"
    );

    [[nodiscard]] http::message_generator operator()(
        const HttpRequest& request
    ) const;

private:
    std::filesystem::path root_;
    std::string index_file_;

    [[nodiscard]] std::optional<std::filesystem::path>
    resolve(std::string_view request_path) const;

    [[nodiscard]] static bool is_inside(
        const std::filesystem::path& root,
        const std::filesystem::path& target
    ) noexcept;
};

}  // namespace Revres
