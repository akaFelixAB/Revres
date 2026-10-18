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

#include "HttpTypes.hpp"

namespace Revres {

class Router final {
public:
    Router() = default;

    void add_route(
        http::verb method,
        const std::string& pattern,
        const Handler& handler
    );

    [[nodiscard]] std::optional<MatchResult> match(
        http::verb method,
        std::string_view target
    ) const;

private:
    // Radix Tree
    enum class NodeType {
        Static,
        Parameter,
        CatchAll
    };

    struct Node {
        // Type of the node: static, parameter, or catch-all.
        NodeType type_ = NodeType::Static;
        // Compressed literal text.
        // Empty for parameter/catch-all nodes and the root.
        std::string prefix_;

        // Static children nodes in the radix tree.
        std::vector<std::unique_ptr<Node>> static_;

        // Parameter child node in the radix tree
        std::unique_ptr<Node> parameter_;

        // Catch-all child node in the radix tree
        std::unique_ptr<Node> catch_all_;

        // Present only when a complete route ends at this node.
        std::optional<Route> route_;
    };

    // Root nodes for each HTTP method.
    std::map<http::verb, std::unique_ptr<Node>> roots_;

    // Insert a static string into the radix tree.
    Node* insert_static(
        Node& parent,
        std::string_view text
    );

    // Insert a parameter or catch-all token into the radix tree.
    Node* insert_special(
        Node& parent,
        const NodeType type
    );

    void insert_route(
        http::verb method,
        const std::string& pattern,
        const Handler& handler
    );
};

}  // namespace Revres