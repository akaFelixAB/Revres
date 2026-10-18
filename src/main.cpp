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

#include "ConfigLoader.hpp"
#include "HttpServer.hpp"
#include "Logger.hpp"

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>  // For handling signals like SIGINT and SIGTERM
#include <boost/beast.hpp>

// Handle termination signals for graceful shutdown
void handle_signal(boost::asio::io_context& ioc, boost::system::error_code ec, int signum) {
    if (!ec) {
        spdlog::warn("Received termination signal ({}), stopping server...", signum);
        ioc.stop();
    }
}

int main(int argc, char* argv[]) {
    namespace http = boost::beast::http;
    namespace net = boost::asio;
    using tcp = net::ip::tcp;

    std::string config_path = "default.toml";

    if (argc > 1) {
        config_path = argv[1];
    }

    // Logger instance for logging throughout the application
    Revres::Logger logger;
    // Initialize bootstrap logging for early startup messages
    logger.initialize_bootstrap();

    try {
        Revres::ConfigLoader config_loader;
        auto cfg = config_loader.load_file(config_path);

        // Initialize logging
        logger.initialize(cfg);

        net::io_context ioc;

        // Create the HTTP server
        Revres::HttpServer server(ioc, cfg);

        server.run();

        // Set up signal handling for graceful shutdown
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait(boost::beast::bind_front_handler(&handle_signal, std::ref(ioc)));

        // Run the I/O context to start processing events
        spdlog::info("Server event loop starting...");
        ioc.run();
        spdlog::info("Server event loop stopped cleanly.");
    } catch (const std::exception& e) {
        spdlog::critical("Exception: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("Flushing logs and shutting down...");
    return EXIT_SUCCESS;
}
