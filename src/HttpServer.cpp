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

#include "HttpServer.hpp"

#include "HttpSession.hpp"
#include "ResponseFactory.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>

namespace Revres {

http::message_generator HttpServer::handle_static_request(
    const http::request<http::string_body>& req,
    const std::string& root_dir,
    const std::string& index_file
) {
    // Parse the request target (e.g., "/path/to/file?query=1")
    auto result = boost::urls::parse_origin_form(req.target());
    if (result.has_error()) {
        spdlog::warn("Invalid URL format: {}", req.target());
        return ResponseFactory::bad_request(
            req.version(),
            req.keep_alive(),
            "400 Bad Request: Invalid URL format"
        );
    }

    boost::urls::url_view uv = result.value();

    spdlog::debug("Parsed URL: {}", uv.buffer());

    // Get the path and query components (e.g., "/path/to/file" and "query=1")
    // std::string path = uv.path();
    // std::string query = uv.query();

    // CHeck for ".." in the path segments to prevent directory traversal
    bool contains_dot_dot =
        std::any_of(uv.segments().begin(), uv.segments().end(), [](std::string_view seg) {
            return seg == "..";
        });

    if (contains_dot_dot) {
        spdlog::warn("Illegal request-target detected: {}", req.target());
        return ResponseFactory::forbidden(
            req.version(),
            req.keep_alive(),
            "403 Forbidden: Illegal request-target"
        );
    }

    // Extract the pure Path component (ignore Query parameters)
    std::string req_path = uv.path();

    // Handle the case where the request path is empty or ends with a slash, indicating a directory
    if (req_path.empty() || req_path.back() == '/') {
        req_path += index_file;
    }

    // Construct the full file path by combining the root directory and the request path
    // Convert into relative path to prevent filesystem traversal
    std::filesystem::path rel_path = req_path.starts_with('/') ? req_path.substr(1) : req_path;
    std::filesystem::path base_path = std::filesystem::weakly_canonical(root_dir);
    std::filesystem::path target_path = std::filesystem::weakly_canonical(base_path / rel_path);

    // Check if the target path is still within the base path to prevent directory traversal
    auto [root_end, dummy] = std::mismatch(base_path.begin(), base_path.end(), target_path.begin());
    if (root_end != base_path.end()) {
        spdlog::warn("Path traversal attempt blocked: {}", target_path.string());
        return ResponseFactory::forbidden(
            req.version(),
            req.keep_alive(),
            "403 Forbidden: Path traversal attempt"
        );
    }

    std::string full_path = target_path.string();

    // Read the file and prepare the response
    beast::error_code ec;
    http::file_body::value_type body;
    body.open(full_path.c_str(), beast::file_mode::scan, ec);

    if (ec == beast::errc::no_such_file_or_directory) {
        spdlog::warn("File not found: {}", full_path);
        return ResponseFactory::not_found(
            req.version(),
            req.keep_alive()
        );
    }

    if (ec) {
        spdlog::error("Server Error: {}", ec.message());
        return ResponseFactory::internal_server_error(
            req.version(),
            req.keep_alive()
        );
    }

    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())
    };

    res.set(http::field::content_type, mime_type(full_path));
    res.keep_alive(req.keep_alive());
    res.prepare_payload();

    return res;
}

void HttpServer::load_routes_from_config(const ServerConfig& config) {
    spdlog::info("Loading routes from configuration");
    for (const auto& r : config.routes_) {
        spdlog::info("Loading route: {}", r.path_);
        if (r.type_ == RouteType::Static) {
            // Register a static file handler for the route
            pimpl_->route(r.path_, [this, r](const http::request<http::string_body>& req) {
                return handle_static_request(req, r.root_, r.index_);
            });
        } else if (r.type_ == RouteType::Proxy) {
            // Register a proxy handler for the route
            spdlog::warn("Proxy route handling is not implemented yet");
        }
    }
}

void HttpServer::Impl::on_accept(boost::system::error_code ec, tcp::socket socket) {
    if (!ec) {
        std::make_shared<HttpSession>(std::move(socket), routes_)->start();
    }
    accept_request();
    spdlog::info("New connection accepted");
}

void HttpServer::Impl::accept_request() {
    acceptor_.async_accept(
        beast::bind_front_handler(
            &Impl::on_accept,
            shared_from_this()  // safe call to shared_from_this()
        )
    );
}

}  // namespace Revres
