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

#include "Router.hpp"
#include "ResponseFactory.hpp"
#include "HttpRequest.hpp"

#include <catch2/catch_test_macros.hpp>

// Test routes:
// /users/static
// /username/static
// /username/:id
// /username/:id/profile
// /username/:UserId/settings
// /username
// /username/*path


TEST_CASE(
    "Router::match returns correct route for known patterns",
    "[router][match]"
) {
    using namespace Revres;
    Router router;

    Handler handler = [](const HttpRequest& req) {
        return ResponseFactory::ok(
            req.raw().version(),
            req.raw().keep_alive(),
            "OK"
        );
    };

    // Add a routes
    for (const auto& pattern : {
        "/users/static",
        "/username/static",
        "/username/:id",
        "/username/:id/profile",
        "/username/:UserId/settings",
        "/username",
        "/username/*path"
    }) {
        router.add_route(
            http::verb::get,
            pattern,
            handler
        );
    }

    SECTION(
        "Router::match returns correct route for static patterns", 
        "[router][match][static]"
    ) {
        // Test matching static route
        REQUIRE(
            router.match(http::verb::get, "/users/static").has_value()
        );
        CHECK(
            router.match(http::verb::get, "/users/static")
                .value()
                .route_->
                parameter_names_.empty()
        );

        // Test matching non-existent route
        CHECK(
            !router.match(http::verb::get, "/users/unknown").has_value()
        );

        // Test matching the new static routes
        REQUIRE(
            router.match(http::verb::get, "/username").has_value()
        );
        CHECK(
            router.match(http::verb::get, "/username")
                .value()
                .parameter_values_.empty()  // This is a static route, so no parameters should be captured
        );
        REQUIRE(
            router.match(http::verb::get, "/username/static").has_value()
        );
    }
    
    SECTION(
        "Router::match returns correct route for parameterized patterns", 
        "[router][match][parameterized]"
    ) {
        // Test matching the parameter route
        REQUIRE(
            router.match(http::verb::get, "/username/abc").has_value()
        );
        CHECK(
            router.match(http::verb::get, "/username/abc")
                .value()
                .route_->
                parameter_names_[0] == "id"
        );
        CHECK(
            router.match(http::verb::get, "/username/abc")
                .value()
                .route_->
                parameter_names_.size() == 1
        );

        // Test matching the parameter route
        CHECK(
            router.match(http::verb::get, "/username/123")
                .value()
                .parameter_values_[0] == "123"
        );

        // Test matching the second parameter route
        CHECK(
            router.match(http::verb::get, "/username/456/profile")
                .value()
                .parameter_values_[0] == "456"
        );
        CHECK(
            router.match(http::verb::get, "/username/456/profile")
                .value()
                .route_->
                parameter_names_[0] == "id"
        );

        // Test matching the third parameter route
        REQUIRE(
            router.match(http::verb::get, "/username/789/settings").has_value()
        );
        CHECK(
            router.match(http::verb::get, "/username/789/settings")
                .value()
                .parameter_values_[0] == "789"
        );
        CHECK(
            router.match(http::verb::get, "/username/789/settings")
                .value()
                .route_->
                parameter_names_[0] == "UserId"
        );
    }

    SECTION(
        "Router::match returns correct route for catch-all patterns", 
        "[router][match][catch-all]"
    ) {
        // Test matching the catch-all route
        REQUIRE(
            router.match(http::verb::get, "/username/anything/here").has_value()
        );
        CHECK(
            router.match(http::verb::get, "/username/anything/here")
                .value()
                .route_->
                parameter_names_[0] == "path"  // Catch-all parameter name is "*path"
        );
        CHECK(
            router.match(http::verb::get, "/username/anything/here")
                .value()
                .parameter_values_[0] == "anything/here"  // Catch-all captures the rest of the path
        );

        // A trailing slash is normalized to the registered /username route.
        REQUIRE(
            router.match(http::verb::get, "/username/").has_value()
        );
    }

    SECTION(
        "Router::match ignores query strings when matching routes",
        "[router][match][query-string]"
    ) {
        // Test matching the query string with the static route
        REQUIRE(
            router.match(http::verb::get, "/users/static?query=1").has_value()
        );
        CHECK(
            router.match(http::verb::get, "/users/static?query=1")
                .value()
                .route_->
                parameter_names_.empty()
        );

        // Test matching the query string with the catch-all route
        REQUIRE(
            router.match(http::verb::get, "/username/anything/here?query=1").has_value()
        );
        CHECK(
            router.match(http::verb::get, "/username/anything/here?query=1")
                .value()
                .route_->
                parameter_names_[0] == "path"  // Catch-all parameter name is "*path"
        );
        CHECK(
            router.match(http::verb::get, "/username/anything/here?query=1")
                .value()
                .parameter_values_[0] == "anything/here"  // Catch-all captures the rest of the path
        );
    }
};

TEST_CASE(
    "Router::match distinguishes unsupported methods from missing paths",
    "[router][match][method]"
) {
    using namespace Revres;
    Router router;

    Handler handler = [](const HttpRequest& req) {
        return ResponseFactory::ok(
            req.raw().version(),
            req.raw().keep_alive(),
            "OK"
        );
    };

    router.add_route(http::verb::get, "/resource", handler);

    const auto unsupported = router.match(
        http::verb::post,
        "/resource"
    );

    REQUIRE(unsupported.has_value());
    CHECK(unsupported->route_ == nullptr);
    CHECK(unsupported->allowed_methods_ == "GET");

    CHECK_FALSE(
        router.match(http::verb::post, "/missing").has_value()
    );
}

TEST_CASE(
    "Router exposes path parameters by name and freezes route registration",
    "[router][match][parameters][freeze]"
) {
    using namespace Revres;
    Router router;

    Handler handler = [](const HttpRequest& req) {
        return ResponseFactory::ok(
            req.raw().version(),
            req.raw().keep_alive(),
            "OK"
        );
    };

    router.add_route(http::verb::get, "/users/:id", handler);

    const auto matched = router.match(http::verb::get, "/users/42/");
    REQUIRE(matched.has_value());
    REQUIRE(matched->parameter("id").has_value());
    CHECK(*matched->parameter("id") == "42");
    CHECK_FALSE(matched->parameter("missing").has_value());

    router.freeze();
    CHECK_THROWS_AS(
        router.add_route(http::verb::get, "/after-freeze", handler),
        std::logic_error
    );
}

TEST_CASE("Router::add_route throws for invalid patterns", "[router][pattern-normalization]") {
    using namespace Revres;
    Router router;

    Handler handler = [](const HttpRequest& req) {
        return ResponseFactory::ok(
            req.raw().version(),
            req.raw().keep_alive(),
            "OK"
        );
    };

    // Test invalid patterns
    for (const auto& pattern : {
        "",  // Empty pattern
        "no-leading-slash",             // No leading slash
        "/invalid/segment/..",          // Dot segment
        "/invalid/segment/.",           // Dot segment
        "/invalid/:param/..",           // Dot segment with parameter
        "/invalid/:param/.",            // Dot segment with parameter
        "/invalid/*catch/all",          // Catch-all not last
        "/invalid/*catch/*all",         // Multiple catch-all segments
        "/invalid/:param/*catch/all"    // Catch-all not last with parameter
    }) {
        CHECK_THROWS_AS(
            router.add_route(http::verb::get, pattern, handler),
            std::invalid_argument
        );
    }
}
