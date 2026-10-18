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

#include "ConfigValidator.hpp"
#include "ConfigDiagnostic.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <string_view>

namespace {

Revres::ServerConfig valid_config() {
    Revres::ServerConfig config;

    config.name_ = "TestServer";
    config.host_ = "127.0.0.1";
    config.port_ = 8080;
    config.timeout_seconds_ = 30;
    config.max_connections_ = 100;

    Revres::RouteConfig route;
    route.path_ = "/";
    route.type_ = Revres::RouteType::Static;
    route.root_ = ".";
    route.index_ = "index.html";

    config.routes_.push_back(std::move(route));

    return config;
}

bool has_field(
    const std::vector<Revres::ConfigDiagnostic>& diagnostics,
    std::string_view field
) {
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [field](const Revres::ConfigDiagnostic& diagnostic) {
            return diagnostic.field_ == field;
        }
    );
}

const Revres::ConfigDiagnostic* find_diagnostic(
    const std::vector<Revres::ConfigDiagnostic>& diagnostics,
    std::string_view field
) {
    const auto it = std::find_if(
        diagnostics.begin(),
        diagnostics.end(),
        [field](const Revres::ConfigDiagnostic& diagnostic) {
            return diagnostic.field_ == field;
        }
    );

    if (it == diagnostics.end()) {
        return nullptr;
    }

    return &*it;
}

}  // namespace

TEST_CASE("Valid configuration produces no diagnostics", "[config][validator]") {
    const auto config = valid_config();

    const auto diagnostics =
        Revres::ConfigValidator::validate(config);

    CHECK(diagnostics.empty());
}

TEST_CASE("Port must be between 1 and 65535", "[config][validator]") {
    SECTION("Zero is invalid", "[config][validator]") {
        auto config = valid_config();
        config.port_ = 0;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].field_ == "server.port");
        CHECK(
            diagnostics[0].message_ ==
            "must be between 1 and 65535"
        );
    }

    SECTION("Negative value is invalid", "[config][validator]") {
        auto config = valid_config();
        config.port_ = -1;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].field_ == "server.port");
        CHECK(
            diagnostics[0].message_ ==
            "must be between 1 and 65535"
        );
    }

    SECTION("Above maximum is invalid", "[config][validator]") {
        auto config = valid_config();
        config.port_ = 65536;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].field_ == "server.port");
        CHECK(
            diagnostics[0].message_ ==
            "must be between 1 and 65535"
        );
    }

    SECTION("Minimum valid value") {
        auto config = valid_config();
        config.port_ = 1;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK_FALSE(has_field(diagnostics, "server.port"));
    }

    SECTION("Maximum valid value") {
        auto config = valid_config();
        config.port_ = 65535;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK_FALSE(has_field(diagnostics, "server.port"));
    }
}

TEST_CASE("Timeout must be greater than zero", "[config][validator]") {
    SECTION("Zero is invalid") {
        auto config = valid_config();
        config.timeout_seconds_ = 0;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        const auto* diagnostic =
            find_diagnostic(diagnostics, "server.timeout");

        REQUIRE(diagnostic != nullptr);
        CHECK(diagnostic->message_ == "must be greater than 0");
    }

    SECTION("Negative value is invalid") {
        auto config = valid_config();
        config.timeout_seconds_ = -1;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK(has_field(diagnostics, "server.timeout"));
    }

    SECTION("Positive value is valid") {
        auto config = valid_config();
        config.timeout_seconds_ = 1;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK_FALSE(has_field(diagnostics, "server.timeout"));
    }
}

TEST_CASE(
    "max_connections must be greater than zero",
    "[config][validator]"
) {
    SECTION("Zero is invalid") {
        auto config = valid_config();
        config.max_connections_ = 0;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK(has_field(
            diagnostics,
            "server.max_connections"
        ));
    }

    SECTION("Negative value is invalid") {
        auto config = valid_config();
        config.max_connections_ = -1;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK(has_field(
            diagnostics,
            "server.max_connections"
        ));
    }

    SECTION("Positive value is valid") {
        auto config = valid_config();
        config.max_connections_ = 1;

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK_FALSE(has_field(
            diagnostics,
            "server.max_connections"
        ));
    }
}

TEST_CASE("Host must be valid", "[config][validator]") {
    SECTION("empty host") {
        auto config = valid_config();
        config.host_.clear();

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        const auto* diagnostic =
            find_diagnostic(diagnostics, "server.host");

        REQUIRE(diagnostic != nullptr);
        CHECK(diagnostic->message_ == "must not be empty");
    }

    SECTION("Invalid IP address") {
        auto config = valid_config();
        config.host_ = "not-an-ip-address";

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK(has_field(diagnostics, "server.host"));
    }

    SECTION("Valid IPv4 address") {
        auto config = valid_config();
        config.host_ = "192.168.1.10";

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK_FALSE(has_field(diagnostics, "server.host"));
    }

    SECTION("Valid IPv6 address") {
        auto config = valid_config();
        config.host_ = "::1";

        const auto diagnostics =
            Revres::ConfigValidator::validate(config);

        CHECK_FALSE(has_field(diagnostics, "server.host"));
    }
}

TEST_CASE("Route path must start with slash", "[config][routes]") {
    auto config = valid_config();

    REQUIRE(config.routes_.size() == 1);

    config.routes_[0].path_ = "api";

    const auto diagnostics =
        Revres::ConfigValidator::validate(config);

    const auto* diagnostic =
        find_diagnostic(diagnostics, "routes[0].path");

    REQUIRE(diagnostic != nullptr);
    CHECK(diagnostic->message_ == "must start with '/'");
}

TEST_CASE("Route path must not be empty", "[config][routes]") {
    auto config = valid_config();
    config.routes_[0].path_.clear();

    const auto diagnostics =
        Revres::ConfigValidator::validate(config);

    const auto* diagnostic =
        find_diagnostic(diagnostics, "routes[0].path");

    REQUIRE(diagnostic != nullptr);
    CHECK(diagnostic->message_ == "must not be empty");
}

TEST_CASE("Route diagnostic contains route index", "[config][routes]") {
    auto config = valid_config();

    Revres::RouteConfig second_route;
    second_route.path_ = "api";
    second_route.type_ = Revres::RouteType::Static;
    second_route.root_ = ".";

    config.routes_.push_back(std::move(second_route));

    const auto diagnostics =
        Revres::ConfigValidator::validate(config);

    CHECK(has_field(diagnostics, "routes[1].path"));
    CHECK_FALSE(has_field(diagnostics, "routes[0].path"));
}

TEST_CASE("Proxy route is rejected as unimplemented", "[config][routes]") {
    auto config = valid_config();

    config.routes_[0].type_ = Revres::RouteType::Proxy;
    config.routes_[0].upstream_ = "http://127.0.0.1:3000";

    const auto diagnostics =
        Revres::ConfigValidator::validate(config);

    const auto* diagnostic =
        find_diagnostic(diagnostics, "routes[0].type");

    REQUIRE(diagnostic != nullptr);
    CHECK(
        diagnostic->message_ ==
        R"(route type "proxy" is not implemented)"
    );
}

TEST_CASE("Static route requires root", "[config][routes]") {
    auto config = valid_config();
    config.routes_[0].root_.clear();

    const auto diagnostics =
        Revres::ConfigValidator::validate(config);

    const auto* diagnostic =
        find_diagnostic(diagnostics, "routes[0].root");

    REQUIRE(diagnostic != nullptr);
    CHECK(
        diagnostic->message_ ==
        "must not be empty for static routes"
    );
}

TEST_CASE("Static route root must exist", "[config][routes]") {
    auto config = valid_config();
    config.routes_[0].root_ =
        "/this/path/should/not/exist/revres-test";

    const auto diagnostics =
        Revres::ConfigValidator::validate(config);

    CHECK(has_field(diagnostics, "routes[0].root"));
}

TEST_CASE(
    "Validator reports all invalid fields",
    "[config][validator]"
) {
    auto config = valid_config();

    config.host_ = "invalid-host";
    config.port_ = 70000;
    config.timeout_seconds_ = 0;
    config.max_connections_ = -1;
    config.routes_[0].path_ = "api";
    config.routes_[0].type_ = Revres::RouteType::Proxy;
    config.routes_[0].upstream_ = "invalid-url";

    const auto diagnostics =
        Revres::ConfigValidator::validate(config);

    CHECK(has_field(diagnostics, "server.host"));
    CHECK(has_field(diagnostics, "server.port"));
    CHECK(has_field(diagnostics, "server.timeout"));
    CHECK(has_field(diagnostics, "server.max_connections"));
    CHECK(has_field(diagnostics, "routes[0].path"));
    CHECK(has_field(diagnostics, "routes[0].type"));
    CHECK(has_field(diagnostics, "routes[0].upstream"));
}