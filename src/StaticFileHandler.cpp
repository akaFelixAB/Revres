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

#include "StaticFileHandler.hpp"

#include "HttpRequest.hpp"
#include "MimeTypes.hpp"
#include "ResponseFactory.hpp"
#include "HttpTypes.hpp"

#include <boost/beast/http.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <system_error>

namespace Revres {

namespace beast = boost::beast;

StaticFileHandler::StaticFileHandler(
    const std::filesystem::path& root,
    const std::string& index_file
) :
    root_(std::filesystem::weakly_canonical(root)),
    index_file_(index_file) {}

// Check if the target path is inside the root directory.
bool StaticFileHandler::is_inside(
    const std::filesystem::path& root,
    const std::filesystem::path& target
) noexcept {
    auto root_it = root.begin();
    auto target_it = target.begin();


    // Iterate through both paths and compare each component. 
    // If any component of the target path does not match the 
    // corresponding component of the root path, return false.
    for (; root_it != root.end(); root_it++, target_it++) {
        if (target_it == target.end() || *root_it != *target_it) {
            return false;
        }
    }

    return true;
}

// Resolve the request path to an absolute path within the root directory.
std::optional<std::filesystem::path>
StaticFileHandler::resolve(std::string_view request_path) const {
    if (request_path.empty() || request_path.front() != '/') {
        return std::nullopt;
    }

    std::filesystem::path relative(request_path);

    // Remove leading slash to make it relative to the root directory.
    if (relative.is_absolute()) {
        relative = relative.relative_path();
    }

    // Check for path traversal attempts (e.g., ".." or ".") in the request path.
    for (const auto& part : relative) {
        if (part == "." || part == "..") {
            return std::nullopt;
        }
    }

    // If the request path is a directory, append the index file.
    if (relative.empty() || request_path.back() == '/') {
        relative /= index_file_;
    }

    std::error_code ec;
    auto target = std::filesystem::weakly_canonical(root_ / relative, ec);

    // If there was an error during canonicalization or the target 
    // is not inside the root, return std::nullopt.
    if (ec || !is_inside(root_, target)) {
        return std::nullopt;
    }

    return target;
}

http::message_generator StaticFileHandler::operator()(
    const HttpRequest& request
) const {
    const auto method = request.method();

    // Only handle GET and HEAD requests for static files.
    if (method != http::verb::get &&
        method != http::verb::head) {
        spdlog::warn(
            "Static file handler received unsupported method: {}",
            http::to_string(method)
        );

        return ResponseFactory::method_not_allowed(
            request.raw().version(),
            request.raw().keep_alive(),
            "GET, HEAD"
        );
    }

    const auto target = resolve(request.path());

    if (!target.has_value()) {
        spdlog::warn(
            "Static file path escaped root: {}",
            request.path()
        );

        return ResponseFactory::forbidden(
            request.raw().version(),
            request.raw().keep_alive(),
            "403 Forbidden"
        );
    }

    // Check that the resolved path is a regular file before opening it.
    std::error_code status_ec;
    const auto file_status = std::filesystem::status(target.value(), status_ec);

    // The requested index/file does not exist.
    if (file_status.type() == std::filesystem::file_type::not_found) {
        spdlog::debug(
            "Static file not found: {}",
            target.value().string()
        );

        return ResponseFactory::not_found(
            request.raw().version(),
            request.raw().keep_alive()
        );
    }

    // We could not determine whether the path exists.
    if (status_ec ||
        file_status.type() == std::filesystem::file_type::none) {
        spdlog::error(
            "Failed to stat static file path {}: {}",
            target.value().string(),
            status_ec.message()
        );

        return ResponseFactory::internal_server_error(
            request.raw().version(),
            request.raw().keep_alive()
        );
    }

    // The path exists, but is a directory, socket, symlink target, etc.
    if (!std::filesystem::is_regular_file(file_status)) {
        if (std::filesystem::is_directory(file_status)) {
            spdlog::debug(
                "Static path is a directory: {}",
                target.value().string()
            );
        }

        return ResponseFactory::not_found(
            request.raw().version(),
            request.raw().keep_alive()
        );
    }


    boost::system::error_code ec;
    http::file_body::value_type body;

    body.open(
        target.value().c_str(),
        beast::file_mode::scan,
        ec
    );

    if (ec == beast::errc::no_such_file_or_directory) {
        spdlog::debug(
            "Static file not found: {}",
            target.value().string()
        );

        return ResponseFactory::not_found(
            request.raw().version(),
            request.raw().keep_alive()
        );
    }

    if (ec) {
        spdlog::error(
            "Failed to open static file {}: {}",
            target.value().string(),
            ec.message()
        );

        return ResponseFactory::internal_server_error(
            request.raw().version(),
            request.raw().keep_alive()
        );
    }

    const auto content_type =
        MimeTypes::from_path(target.value().string());

    // HEAD responses contain the same metadata as GET responses,
    // including Content-Length, but contain no message body.
    if (method == http::verb::head) {
        http::response<http::empty_body> response{
            http::status::ok,
            request.raw().version()
        };

        response.set(
            http::field::content_type,
            content_type
        );

        response.content_length(body.size());
        response.keep_alive(request.raw().keep_alive());

        return http::message_generator{std::move(response)};
    }

    http::response<http::file_body> response{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(
            http::status::ok,
            request.raw().version()
        )
    };

    response.set(
        http::field::content_type,
        content_type
    );

    response.keep_alive(request.raw().keep_alive());
    response.prepare_payload();

    return http::message_generator{std::move(response)};
}

}  // namespace Revres
