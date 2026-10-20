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
#include "StaticFileHandler.hpp"
#include "HttpRequest.hpp"

#include <boost/url.hpp>

#include <spdlog/spdlog.h>

namespace Revres {

HttpSession::HttpSession(
    tcp::socket&& socket, 
    std::shared_ptr<const Router> router,
    int timeout_seconds
) :
    socket_(std::move(socket)), 
    router_(std::move(router)),
    timeout_seconds_(timeout_seconds) {}

void HttpSession::on_read(
    boost::system::error_code ec,
    std::size_t
) {
    if (ec == http::error::end_of_stream) {
        boost::system::error_code shutdown_ec;
        socket_.socket().shutdown(
            tcp::socket::shutdown_send,
            shutdown_ec
        );
        return;
    }

    if (ec) {
        spdlog::error(
            "Failed to read request: {}",
            ec.message()
        );
        return;
    }

    process_request();
}

void Revres::HttpSession::read_request() {
    request_ = {};  // Clear the previous request

    http::async_read(
        socket_,
        buffer_,
        request_,
        beast::bind_front_handler(&HttpSession::on_read, shared_from_this())
    );
}

void Revres::HttpSession::process_request() {
    socket_.expires_after(
        std::chrono::seconds(timeout_seconds_)
    );

    // Save the keep-alive status and the request path for routing
    const bool keep_alive = request_.keep_alive();
    // Save the version for the response
    const auto version = request_.version();

    // Parse the request target (e.g., "/path/to/file?query=1")
    HttpRequest http_request(std::move(request_));

    auto parsed_url = http_request.url();

    std::string path = parsed_url.path();

    if (!router_) {
        throw std::invalid_argument("HttpSession requires a Router");
    }

    auto matched = router_->match(
        http_request.method(),
        http_request.path()
    );

    http::message_generator msg = ResponseFactory::internal_server_error(
        version,
        keep_alive,
        "Internal Server Error"
    );

    if (!matched) {
        msg = ResponseFactory::not_found(
            version,
            keep_alive,
            "Not Found"
        );
    } else if (matched->route_ == nullptr) {
        msg = ResponseFactory::method_not_allowed(
            version,
            keep_alive,
            matched->allowed_methods_,
            "405 Method Not Allowed"
        );
    } else {
        http_request.set_path_parameters(
            matched->route_->parameter_names_,
            matched->parameter_values_
        );
        msg = matched->route_->handler_(http_request);
    }

    // Write the response back to the client
    auto self = shared_from_this();
    beast::async_write(
        socket_,
        std::move(msg),
        [self, keep_alive](
            boost::system::error_code ec,
            std::size_t
        ) {
            if (ec) {
                spdlog::error(
                    "Failed to write response: {}",
                    ec.message()
                );
                return;
            }

            if (keep_alive) {
                self->read_request();
            }
        }
    );
}

}  // namespace Revres
