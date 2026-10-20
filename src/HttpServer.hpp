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
#include "Router.hpp"
#include "HttpTypes.hpp"
#include "MimeTypes.hpp"

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

class HttpServer final {
private:
    tcp::acceptor acceptor_;
    ServerConfig config_;
    std::shared_ptr<Router> router_;

public:
    HttpServer(net::io_context& ioc, const ServerConfig& config);

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void route(RouteConfig& route_config);
    void run();
    void load_routes_from_config(ServerConfig& config);

private:
    void on_accept(
        boost::system::error_code ec,
        tcp::socket socket
    );

    void accept_request();
};

}  // namespace Revres