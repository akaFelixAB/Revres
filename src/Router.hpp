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

#include <atomic>

namespace Revres {

class Router final {
public:
    Router() = default;

    void add_route(
        http::verb method,
        const std::string& pattern,
        const Handler& handler
    );

    // Call after all routes have been registered and before publishing Router.
    void freeze() noexcept;

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

        // This is a map of HTTP methods to their corresponding routes.
        std::unordered_map<http::verb, std::shared_ptr<const Route>> routes_;

        // Cached Allow header value for this node.
        std::string allowed_methods_;
    };

    struct PathMatch {
        const Node* node_;
        std::vector<std::string_view> parameter_values_;
    };

    // Root node for all HTTP methods.
    Node roots_;
    std::atomic<bool> frozen_ = false;

    // Insert a static string into the radix tree.
    Node* insert_static(
        Node& parent,
        std::string_view text
    );

    void split_static_child(
        Node& child,
        std::size_t prefix_length
    );

    // Insert a parameter or catch-all token into the radix tree.
    Node* insert_special(
        Node& parent,
        const NodeType type
    );

    void insert_route(
        http::verb method,
        std::string_view pattern,
        std::vector<std::string> parameter_names,
        const Handler& handler
    );

    [[nodiscard]] std::optional<PathMatch> find_path(
        std::string_view target
    ) const;

    // Match the method and path based on the radix tree
    // Returns a MatchResult if a match is found, otherwise returns std::nullopt
    std::optional<MatchResult> match_method(
        const Node& node,
        http::verb method,
        std::vector<std::string_view>&& parameter_values
    ) const;
};

}  // namespace Revres