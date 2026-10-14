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

#include "MimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "MimeTypes::from_path returns correct MIME type for known extensions",
    "[mime][lookup]"
) {
    CHECK(
        VeloxServ::MimeTypes::from_path("index.html") ==
        "text/html"
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("style.css") ==
        "text/css"
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("script.js") ==
        "application/javascript"
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("image.png") ==
        "image/png"
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("/styles/style.css") ==
        "text/css"
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("/images/logo.png") ==
        "image/png"
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("/logo.png/image") ==
        "application/octet-stream"  // Not a valid use case, but should return default MIME type
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("/images/logo.png/") ==
        "application/octet-stream"  // Not a valid use case, but should return default MIME type
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("/images/logo.PNG") ==
        "image/png"
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("/images/logo.PnG") ==
        "image/png"
    );
    CHECK(
        VeloxServ::MimeTypes::from_path("/index") ==
        "text/html"  // No extension, but should return default MIME type for index files
    );
}