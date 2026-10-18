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

#include "ConfigLoader.hpp"
#include "HttpTypes.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/url.hpp>

#include <spdlog/spdlog.h>

namespace Revres {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view txt) const {
        return std::hash<std::string_view>{}(txt);
    }
};

class HttpServer {
private:
    // The Pimpl idiom: hide implementation details in a separate class
    class Impl : public std::enable_shared_from_this<Impl> {
        tcp::acceptor acceptor_;
        ServerConfig config_;
        std::shared_ptr<RouteTable> routes_;

    public:
        Impl(net::io_context& ioc, ServerConfig config) :
            acceptor_(ioc, tcp::endpoint(net::ip::make_address(config.host_), config.port_)),
            config_(std::move(config)), routes_(std::make_shared<RouteTable>()) {}

        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;
        ~Impl() = default;

        void route(const std::string& path, Handler handler) {
            (*routes_)[path] = std::move(handler);
        }

        void run() {
            spdlog::info("Starting HTTP server");
            spdlog::info("Listening on {}:{}", config_.host_, config_.port_);
            accept_request();
        }

    private:
        void on_accept(boost::system::error_code ec, tcp::socket socket);
        void accept_request();
    };  // class Impl

    // Manage shared_ptr of Impl to ensure proper lifetime management
    std::shared_ptr<Impl> pimpl_;

    // Get the MIME type based on the file extension
    const std::unordered_map<std::string_view, std::string, StringHash, std::equal_to<>> 
        mime_types = {
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
        {".svgz", "image/svg+xml"}
    };

    inline beast::string_view mime_type(beast::string_view path) {
        // Get the file extension from the path
        auto const pos = path.rfind('.');
        if (pos == beast::string_view::npos) {
            return "application/octet-stream";
        }

        beast::string_view const ext = path.substr(pos);

        // Find the MIME type in the map
        auto it = mime_types.find(ext);
        if (it != mime_types.end()) {
            return it->second;
        }

        return "application/octet-stream";
    }

    // The core static file handling function
    http::message_generator handle_static_request(
        const http::request<http::string_body>& req, const std::string& root_dir, const std::string& index_file
    );

public:
    // Automatically create a HttpServer on the heap and return a shared_ptr to it
    HttpServer(net::io_context& ioc, const ServerConfig& config) : pimpl_(std::make_shared<Impl>(ioc, config)) {
        load_routes_from_config(config);
    }

    void load_routes_from_config(const ServerConfig& config);

    // Forward the route and run calls to the Impl instance
    void route(const std::string& path, Handler handler) {
        pimpl_->route(path, std::move(handler));
    }

    void run() {
        pimpl_->run();
    }
};  // class HttpServ

}  // namespace Revres