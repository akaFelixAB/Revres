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

#include "HttpTypes.hpp"
#include "Router.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <memory>

namespace Revres {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Manages the lifetime of an HTTP session for a single connection
class HttpSession : public std::enable_shared_from_this<HttpSession> {
    beast::tcp_stream socket_;                             // Socket for the session
    http::request<http::string_body> request_;            // Request received from the client
    beast::flat_buffer buffer_;                            // Buffer for reading
    std::shared_ptr<const Revres::Router> router_;         // Shared pointer to the router for handling requests
    int timeout_seconds_;

public:
    HttpSession(tcp::socket&& socket, std::shared_ptr<const Router> router, int timeout_seconds = 30);

    inline void start() {
        read_request();
    }

    ~HttpSession() = default;

private:
    // State machine for handling the HTTP session

    // Read the request from the client
    void on_read(boost::system::error_code ec, std::size_t bytes_transferred);
    void read_request();

    // Process the request and generate a response and send it back to the client
    void process_request();
};  // class HttpSession

}  // namespace Revres