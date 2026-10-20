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

#include "routing/Router.hpp"
#include "response/ResponseFactory.hpp"
#include "http/HttpRequest.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("Router::match", "[router][match][benchmark]") {
    using namespace Revres;

    // Handler that returns a simple HTTP 200 OK response
    Handler handler = [](const HttpRequest& req) {
        return ResponseFactory::ok(
            req.raw().version(),
            req.raw().keep_alive(),
            "OK"
        );
    };

    Router router;

    // Define a set of routes to benchmark
    const std::vector<std::pair<std::string, http::verb>> routes = {
        {"/users/static", http::verb::get},
        {"/username/static", http::verb::get},
        {"/username/:id", http::verb::get},
        {"/username/:id/profile", http::verb::get},
        {"/username/:UserId/settings", http::verb::get},
        {"/username/*rest", http::verb::get},

        {"/username/static", http::verb::post},
        {"/username/:id", http::verb::post},
        {"/username/:id/profile", http::verb::post},
    };

    // Register all routes in the router
    for (const auto& [pattern, method] : routes) {
        router.add_route(method, pattern, handler);
    }

    // Test matching the static route
    REQUIRE(router.match(
        http::verb::get, "/username/123/profile"
    ).has_value());

    REQUIRE_FALSE(router.match(
        http::verb::get, "/missing"
    ).has_value());

    BENCHMARK("GET static hit") {
        return router.match(
            http::verb::get, "/username/static"
        );
    };

    BENCHMARK("GET terminal parameter") {
        return router.match(
            http::verb::get, "/username/123"
        );
    };

    BENCHMARK("GET parameter + static suffix") {
        return router.match(
            http::verb::get, "/username/123/profile"
        );
    };

    BENCHMARK("GET static fallback to parameter") {
        return router.match(
            http::verb::get, "/username/static/profile"
        );
    };

    BENCHMARK("GET parameter fallback to catch-all") {
        return router.match(
            http::verb::get, "/username/123/unknown"
        );
    };

    BENCHMARK("POST parameter subtree miss") {
        return router.match(
            http::verb::post, "/username/123/unknown"
        );
    };

    BENCHMARK("GET static prefix miss") {
        return router.match(
            http::verb::get, "/missing"
        );
    };

    BENCHMARK("missing method root") {
        return router.match(
            http::verb::delete_, "/username/123"
        );
    };
}