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

#include "http/HttpSession.hpp"
#include "response/ResponseFactory.hpp"
#include "response/StaticFileHandler.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>

namespace Revres {

void HttpServer::load_routes_from_config(ServerConfig& config) {
    for (auto& route_config : config.routes_) {
        route(route_config);
    }
}

HttpServer::HttpServer(
    net::io_context& ioc,
    const ServerConfig& config
) :
    acceptor_(ioc),
    config_(config),
    router_(std::make_shared<Router>()) {
    const tcp::endpoint endpoint(
        net::ip::make_address(config_.host_),
        static_cast<boost::asio::ip::port_type>(
            config_.port_
        )
    );

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(net::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(net::socket_base::max_listen_connections);

    load_routes_from_config(config_);
    router_->freeze();
}

void HttpServer::route(RouteConfig& route_config) {
    Handler handler;

    switch (route_config.type_) {
    case RouteType::Static:
        handler = StaticFileHandler(
            route_config.root_,
            route_config.index_
        );
        break;

    case RouteType::Proxy:
        spdlog::warn(
            "Proxy route is not implemented: {}",
            route_config.path_
        );
        return;
    }

    for (const auto method : route_config.methods_) {
        spdlog::debug(
            "Registering route: {} {}",
            http::to_string(method),
            route_config.path_
        );
        router_->add_route(
            method,
            route_config.path_,
            handler
        );
    }
}

void HttpServer::run() {
    spdlog::info(
        "Server is running on {}:{}",
        config_.host_,
        config_.port_
    );

    accept_request();
}

void HttpServer::accept_request() {
    acceptor_.async_accept(
        net::make_strand(acceptor_.get_executor()),
        beast::bind_front_handler(
            &HttpServer::on_accept,
            this
        )
    );
}

void HttpServer::on_accept(
    boost::system::error_code ec,
    tcp::socket socket
) {
    if (ec) {
        spdlog::error(
            "Failed to accept connection: {}",
            ec.message()
        );
    } else {
        std::make_shared<HttpSession>(
            std::move(socket),
            router_,
            config_.timeout_seconds_
        )->start();
    }

    accept_request();
}

}  // namespace Revres
