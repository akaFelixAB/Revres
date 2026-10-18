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

#include "HttpSession.hpp"

#include "ResponseFactory.hpp"

#include <boost/url.hpp>

#include <spdlog/spdlog.h>

namespace Revres {

void Revres::HttpSession::on_read(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (!ec) {
        process_request();
    }
}

void Revres::HttpSession::read_request() {
    auto self = shared_from_this();
    http::async_read(
        socket_,
        buffer_,
        request_,
        beast::bind_front_handler(&HttpSession::on_read, shared_from_this())
    );
}

void Revres::HttpSession::process_request() {
    socket_.expires_after(std::chrono::seconds(30));

    // Save the keep-alive status and the request path for routing
    bool keep_alive = request_.keep_alive();

    // Parse the request target (e.g., "/path/to/file?query=1")
    std::string_view req_target = request_.target();
    auto parsed_url = boost::urls::parse_origin_form(req_target);

    std::string path;
    if (parsed_url.has_value()) {
        spdlog::debug("Parsed URL: {}", parsed_url->buffer());
        path.assign(parsed_url->path().data(), parsed_url->path().size());
    } else {
        path.assign(req_target.data(), req_target.size());
    }

    auto matched_it = routes_->end();
    size_t max_len = 0;

    for (auto it = routes_->begin(); it != routes_->end(); it++) {
        const std::string& route_prefix = it->first;
        if (path.rfind(route_prefix, 0) == 0) {  // Match if the path starts with the route prefix
            if (route_prefix.length() > max_len) {
                max_len = route_prefix.length();
                matched_it = it;
            }
        }
    }

    spdlog::debug("Matched route: {}", matched_it != routes_->end() ? matched_it->first : "None");

    http::message_generator msg = ResponseFactory::internal_server_error(
        request_.version(),
        keep_alive,
        "Internal Server Error"
    );

    if (matched_it != routes_->end()) {
        try {
            msg = matched_it->second(request_);  // Handle the request using the registered handler
        } catch (const std::exception& e) {
            spdlog::error("Error handling request: {}", e.what());
            msg = ResponseFactory::internal_server_error(
                request_.version(),
                keep_alive,
                "Internal Server Error"
            );
        }
    } else {
        msg = ResponseFactory::not_found(request_.version(), keep_alive, "404 Not Found");
    }

    // Write the response back to the client
    auto self = shared_from_this();
    beast::async_write(
        socket_,
        std::move(msg),
        [self, keep_alive](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec && keep_alive) {
                self->read_request();
            }
        }
    );
}

}  // namespace Revres
