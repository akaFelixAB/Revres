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

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "Router::match returns correct route for known patterns",
    "[router][match]"
) {
    VeloxServ::Router router;

    SECTION(
        "Router::match returns correct route for static patterns", 
        "[router][match][static]"
    ) {
        // Add a static route
        router.add_route(
            VeloxServ::http::verb::get,
            "/users/static",
            [](const VeloxServ::http::request<VeloxServ::http::string_body>& req) {
                return VeloxServ::http::response<VeloxServ::http::string_body>{
                    VeloxServ::http::status::ok, req.version()
                };
            }
        );

        // Test matching static route
        REQUIRE(
            router.match(VeloxServ::http::verb::get, "/users/static").has_value()
        );
        CHECK(
            router.match(VeloxServ::http::verb::get, "/users/static")
                .value()
                .route_.get()
                .parameter_names_.empty()
        );

        // Test matching non-existent route
        CHECK(
            !router.match(VeloxServ::http::verb::get, "/users/unknown").has_value()
        );

        // Add another static route
        router.add_route(
            VeloxServ::http::verb::get,
            "/username/static",
            [](const VeloxServ::http::request<VeloxServ::http::string_body>& req) {
                return VeloxServ::http::response<VeloxServ::http::string_body>{
                    VeloxServ::http::status::ok, req.version()
                };
            }
        );

        // Test matching the new static route
        REQUIRE(
            router.match(VeloxServ::http::verb::get, "/username/static").has_value()
        );
        CHECK(
            router.match(VeloxServ::http::verb::get, "/username/static")
                .value()
                .route_.get()
                .parameter_names_.empty()
        );
        
        // Test matching the non-existing route again
        CHECK(
            !router.match(VeloxServ::http::verb::get, "/username/unknown").has_value()
        );
    }

    SECTION(
        "Router::match returns correct route for parameterized patterns", 
        "[router][match][parameterized]"
    ) {
        // Add a parameter route
        router.add_route(
            VeloxServ::http::verb::get,
            "/username/:id",
            [](const VeloxServ::http::request<VeloxServ::http::string_body>& req) {
                return VeloxServ::http::response<VeloxServ::http::string_body>{
                    VeloxServ::http::status::ok, req.version()
                };
            }
        );

        // Test matching the parameter route
        REQUIRE(
            router.match(VeloxServ::http::verb::get, "/username/abc").has_value()
        );
        CHECK(
            router.match(VeloxServ::http::verb::get, "/username/abc")
                .value()
                .route_.get()
                .parameter_names_[0] == "id"
        );

        // Test matching the parameter route
        CHECK(
            router.match(VeloxServ::http::verb::get, "/username/123")
                .value()
                .parameter_values_[0] == "123"
        );

        // Add a second parameter route
        router.add_route(
            VeloxServ::http::verb::get,
            "/username/:id/profile",
            [](const VeloxServ::http::request<VeloxServ::http::string_body>& req) {
                return VeloxServ::http::response<VeloxServ::http::string_body>{
                    VeloxServ::http::status::ok, req.version()
                };
            }
        );

        // Test matching the second parameter route
        CHECK(
            router.match(VeloxServ::http::verb::get, "/username/456/profile")
                .value()
                .parameter_values_[0] == "456"
        );
        CHECK(
            router.match(VeloxServ::http::verb::get, "/username/456/profile")
                .value()
                .route_.get()
                .parameter_names_[0] == "id"
        );

        // Add a third parameter route with a the same name but different path
        router.add_route(
            VeloxServ::http::verb::get,
            "/username/:UserId/settings",
            [](const VeloxServ::http::request<VeloxServ::http::string_body>& req) {
                return VeloxServ::http::response<VeloxServ::http::string_body>{
                    VeloxServ::http::status::ok, req.version()
                };
            }
        );

        // Test matching the third parameter route
        REQUIRE(
            router.match(VeloxServ::http::verb::get, "/username/789/settings").has_value()
        );
        CHECK(
            router.match(VeloxServ::http::verb::get, "/username/789/settings")
                .value()
                .parameter_values_[0] == "789"
        );
        CHECK(
            router.match(VeloxServ::http::verb::get, "/username/789/settings")
                .value()
                .route_.get()
                .parameter_names_[0] == "UserId"
        );
    }

    SECTION(
        "Router::match returns correct route for catch-all patterns", 
        "[router][match][catch-all]"
    ) {
        // Add a catch-all route
        router.add_route(
            VeloxServ::http::verb::get,
            "/username/*",
            [](const VeloxServ::http::request<VeloxServ::http::string_body>& req) {
                return VeloxServ::http::response<VeloxServ::http::string_body>{
                    VeloxServ::http::status::ok, req.version()
                };
            }
        );

        // Test matching the catch-all route
        REQUIRE(
            router.match(VeloxServ::http::verb::get, "/username/anything/here").has_value()
        );
        CHECK(
            router.match(VeloxServ::http::verb::get, "/username/anything/here")
                .value()
                .route_.get()
                .parameter_names_[0].empty()  // Catch-all parameter has no name
        );
        CHECK(
            router.match(VeloxServ::http::verb::get, "/username/anything/here")
                .value()
                .parameter_values_[0] == "anything/here"  // Catch-all captures the rest of the path
        );
    }
}
