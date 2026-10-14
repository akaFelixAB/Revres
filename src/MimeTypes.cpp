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

#include "MimeTypes.hpp"

#include <unordered_map>

namespace {

// Only folds ASCII A-Z; does not modify the original string.
constexpr unsigned char ascii_lower(unsigned char ch) noexcept {
    return ch >= 'A' && ch <= 'Z'
        ? static_cast<unsigned char>(ch + ('a' - 'A'))
        : ch;
}

struct ExtensionHash {
    std::size_t operator()(std::string_view text) const noexcept {
        std::size_t hash = 0;

        // Hashes a string_view in a case-insensitive manner.
        for (char ch : text) {
            hash = hash * std::size_t{131}
                 + ascii_lower(static_cast<unsigned char>(ch));
        }

        return hash;
    }
};

struct ExtensionEqual {
    // Compares two string_views for equality in a case-insensitive manner.
    bool operator()(
        std::string_view lhs,
        std::string_view rhs
    ) const noexcept {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        for (std::size_t i = 0; i < lhs.size(); ++i) {
            if (ascii_lower(static_cast<unsigned char>(lhs[i])) !=
                ascii_lower(static_cast<unsigned char>(rhs[i]))) {
                return false;
            }
        }

        return true;
    }
};

using MimeTypesMap = std::unordered_map<
    std::string_view,
    std::string_view,
    ExtensionHash,
    ExtensionEqual
>;

constexpr std::string_view DEFAULT_MIME_TYPE =
    "application/octet-stream";

static const MimeTypesMap MIME_TYPES = {
    {".htm", "text/html"},
    {".html", "text/html"},
    {".php", "text/html"},
    {".css", "text/css"},
    {".txt", "text/plain"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpe", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".gif", "image/gif"},
    {".bmp", "image/bmp"},
    {".ico", "image/vnd.microsoft.icon"},
    {".svg", "image/svg+xml"},
    {".svgz", "image/svg+xml"},
};

}  // namespace

namespace VeloxServ {

std::string_view MimeTypes::from_path(std::string_view path) noexcept {
    // Get the MIME type based on the file extension
    for (auto i = path.size(); i != 0;) {
        const char ch = path[--i];

        if (ch == '/') {
            return DEFAULT_MIME_TYPE;
        }

        if (ch == '.') {
            const auto ext = path.substr(i);
            const auto it = MIME_TYPES.find(ext);

            if (it != MIME_TYPES.end()) {
                return it->second;  // Found a matching MIME type
            }
        }
    }
    return DEFAULT_MIME_TYPE;
}

}  // namespace VeloxServ