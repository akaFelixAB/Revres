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
#include "StaticFileHandler.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>

namespace Revres {

void HttpServer::load_routes_from_config(const ServerConfig& config) {
    spdlog::info("Loading routes from configuration");
    for (const auto& r : config.routes_) {
        spdlog::info("Loading route: {}", r.path_);
        if (r.type_ == RouteType::Static) {
            // Register a static file handler for the route
            pimpl_->route(r.path_, StaticFileHandler(r.root_, r.index_));
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
