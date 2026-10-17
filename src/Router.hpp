#pragma once

#include "HttpTypes.hpp"

namespace VeloxServ {

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

    // Check if a node is a leaf (i.e., it has no children).
    [[nodiscard]] inline bool is_leaf(const Node& node) const {
        return !node.static_.empty() && !node.parameter_ && !node.catch_all_;
    }

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

}  // namespace VeloxServ