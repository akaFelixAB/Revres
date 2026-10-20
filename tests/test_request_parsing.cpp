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

#include "http/HttpRequest.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("HttpRequest parses valid request targets", "[HttpRequest]") {
    using namespace Revres;

    SECTION("Simple path", "[HttpRequest]") {
        http::request<http::string_body> raw;
        raw.target("/foo/bar");

        HttpRequest request(std::move(raw));

        REQUIRE(request.is_valid());
        REQUIRE(request.path() == "/foo/bar");
    }

    SECTION("Path with query parameters", "[HttpRequest]") {
        http::request<http::string_body> raw;
        raw.target("/search?q=hello&lang=en");

        HttpRequest request(std::move(raw));

        REQUIRE(request.is_valid());
        REQUIRE(request.path() == "/search");
        REQUIRE(request.query().find("q") != request.query().end());
        REQUIRE(request.query().find("lang") != request.query().end());
    }

    SECTION("Path with percent-encoded characters", "[HttpRequest]") {
        http::request<http::string_body> raw;
        raw.target("/path%20with%20spaces");

        HttpRequest request(std::move(raw));

        REQUIRE(request.is_valid());
        REQUIRE(request.path() == "/path%20with%20spaces");
    }
}
