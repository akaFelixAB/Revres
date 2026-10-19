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

#include <string_view>
#include <optional>

#include <boost/url.hpp>

#include "HttpTypes.hpp"

namespace Revres {

class HttpRequest {
public:
    enum class Status {
        Ok,
        MalformedTarget,
        InvalidOriginForm,
        InvalidPath
    };

    using RawRequest = http::request<http::string_body>;

    explicit HttpRequest(RawRequest&& raw);
    
    HttpRequest(const RawRequest& raw) = delete;

    [[nodiscard]] Status status() const noexcept { return status_; }
    [[nodiscard]] bool is_valid() const noexcept { return status_ == Status::Ok; }

    [[nodiscard]] http::verb method() const noexcept {
        return raw_.method();
    }

    [[nodiscard]] std::string_view raw_target() const noexcept {
        return raw_.target();
    }

    // Normalized, still percent-encoded path. Use this for routing.
    [[nodiscard]] const std::string_view path() const noexcept {
        const auto encoded = url_.encoded_path();
        return { encoded.data(), encoded.size() };
    }

    // Query parameters.
    [[nodiscard]] boost::urls::params_view query() const noexcept {
        return url_.params();
    }

    [[nodiscard]] std::optional<std::string> query(std::string_view key) const;
    [[nodiscard]] std::vector<std::string> query_all(std::string_view key) const;

    [[nodiscard]] boost::urls::url_view url() const noexcept {
        return boost::urls::url_view(url_);
    }

    [[nodiscard]] const http::request<http::string_body>& raw() const noexcept {
        return raw_;
    }

private:
    RawRequest raw_;

    boost::urls::url url_;
    Status status_ = Status::Ok;
};

}  // namespace Revres