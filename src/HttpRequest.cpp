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

#include "HttpRequest.hpp"

#include <boost/url.hpp>

namespace Revres {

HttpRequest::HttpRequest(
    RawRequest&& raw
) : raw_(std::move(raw)) {
    const auto target = raw_.target();

    if (raw_.method() == http::verb::connect) {
        status_ = Status::InvalidOriginForm;
        return;  // TODO: CONNECT requests are not supported yet.
    }

    const auto parsed = boost::urls::parse_origin_form(target);

    if (!parsed) {
        status_ = Status::MalformedTarget;
        return;
    }

    url_ = parsed.value();

    // Policy: reject dot segments, including percent-encoded forms.
    for (auto segment : url_.segments()) {
        if (segment == "." || segment == "..") {
            status_ = Status::InvalidPath;
            return;
        }
    }

    // normalize only the routing path.
    url_.normalize_path();

    status_ = Status::Ok;
}

std::optional<std::string> HttpRequest::query(std::string_view key) const {
    const auto params = query();

    const auto it = params.find(key);
    if (it == params.end()) {
        return std::nullopt;
    }

    return std::string((*it).value);
}

std::vector<std::string> HttpRequest::query_all(std::string_view key) const {
    const auto params = query();
    std::vector<std::string> values;

    for (const auto& param : params) {
        if (param.key == key) {
            values.emplace_back(param.value);
        }
    }

    return values;
}

}  // namespace Revres
