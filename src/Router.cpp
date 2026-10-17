#include "Router.hpp"

#include <spdlog/spdlog.h>

namespace {

// Find the common prefix of two strings.
static inline std::size_t common_prefix(
    std::string_view a,
    std::string_view b
) {
    std::size_t i = 0;

    while (
        i < a.size() &&
        i < b.size() &&
        a[i] == b[i]
    ) {
        i++;
    }

    return i;
}

}

namespace VeloxServ {

void Router::add_route(
    http::verb method,
    const std::string& pattern,
    const Handler& handler
) {
    insert_route(method, pattern, handler);
}

std::optional<MatchResult> Router::match(
    http::verb method,
    std::string_view target
) const {
    auto it = roots_.find(method);
    if (it == roots_.end() || it->second == nullptr) {
        return std::nullopt;  // No routes since the tree is empty
    }

    std::vector<std::string_view> parameter_values;

    // Traverse the radix tree to find a matching route
    auto search = [&](
        auto&& self,
        const Node* node,
        std::size_t position
    ) -> const Node* {
    // If we have consumed the entire target string and the current node has a route, we have a match.
        if (position == target.size() && node->route_) {
            return node;
        }

        const auto remaining = target.substr(position);

        // Try to match static children first.
        for (const auto& child : node->static_) {
            const std::string_view prefix = child->prefix_;

            // If the remaining target does not start with the child's prefix, skip this child.
            if (remaining.substr(0, prefix.size()) != prefix) {
                continue;
            }

            // If the remaining target starts with the child's prefix, we can try to match it.
            const auto saved = parameter_values.size();

            // Recurse into the static child node.
            if (const Node* matched = self(
                    self,
                    child.get(),
                    position + prefix.size())) {
                return matched;
            }

            // If the static child did not lead to a match, backtrack the parameter values.
            parameter_values.resize(saved);
        }

        // Static children failed, try parameter child.
        if (node->parameter_ &&
            position < target.size() &&
            target[position] != '/'
        ) {
            // Parameter nodes match until the next '/' or the end of the string.
            // Find the next '/' or the end of the string to determine the parameter value.
            auto end = target.find('/', position);
            if (end == std::string_view::npos) {
                // If no '/' is found, the parameter extends to the end of the string.
                end = target.size();
            }

            // Save the current size of parameter_values to backtrack if needed.
            const auto saved = parameter_values.size();

            parameter_values.emplace_back(
                target.substr(position, end - position)
            );

            // Recurse into the parameter child node.
            if (const Node* matched = self(
                self,
                node->parameter_.get(),
                end
            )) {
                return matched;
            }

            parameter_values.resize(saved);
        }

        // If both static and parameter children failed, try catch-all child.
        if (node->catch_all_ && node->catch_all_->route_) {
            // Catch-all matches the rest of the path.
            parameter_values.emplace_back(
                target.substr(position)
            );

            return node->catch_all_.get();
        }

        return nullptr;
    };

    const Node* matched = search(search, it->second.get(), 0);

    if (!matched) {
        return std::nullopt;
    }

    return MatchResult{
        std::cref(matched->route_.value()),
        std::move(parameter_values)
    };
}

Router::Node* Router::insert_static(
    Node& parent,
    std::string_view text
) {
    if (text.empty()) {
        return &parent;
    }

    for (auto& child_ptr : parent.static_) {
        Node& child = *child_ptr;

        if (child.type_ != NodeType::Static ||
            child.prefix_.front() != text.front()) {
            continue;  // Skip non-matching static children
        }

        const auto n = common_prefix(child.prefix_, text);

        // If the common prefix is less than the length of the child's prefix,
        // split the child node.
        if (n < child.prefix_.size()) {
            auto suffix = std::make_unique<Node>();

            suffix->type_ = NodeType::Static;
            suffix->prefix_ = child.prefix_.substr(n);
            suffix->static_ = std::move(child.static_);
            suffix->parameter_ = std::move(child.parameter_);
            suffix->catch_all_ = std::move(child.catch_all_);
            suffix->route_ = std::move(child.route_);

            child.prefix_.resize(n);
            child.static_.clear();
            child.route_.reset();

            child.static_.push_back(std::move(suffix));
        }

        // child.prefix_ is already consumed, so we need to insert the 
        // remaining text into the child node.
        return insert_static(child, text.substr(n));
    }

    // No matching child found, create a new static node.
    auto child = std::make_unique<Node>();
    child->type_ = NodeType::Static;
    child->prefix_ = std::string(text);

    Node* result = child.get();
    parent.static_.push_back(std::move(child));

    // Return the newly created child node for further 
    // insertion of the remaining text.
    return result;
}

Router::Node* Router::insert_special(
    Node& parent,
    const NodeType type
) {
    if (type == NodeType::Static) {
        throw std::invalid_argument("insert_special called with Static type");
    } else if (parent.type_ == NodeType::Parameter || parent.type_ == NodeType::CatchAll) {
        throw std::invalid_argument("Cannot insert a special node under another special node");
    }

    if (type == NodeType::Parameter) {
        if (!parent.parameter_) {
            parent.parameter_ = std::make_unique<Node>();
            parent.parameter_->type_ = NodeType::Parameter;
            return parent.parameter_.get();
        }
        return parent.parameter_.get();
    }

    if (type == NodeType::CatchAll) {
        if (!parent.catch_all_) {
            parent.catch_all_ = std::make_unique<Node>();
            parent.catch_all_->type_ = NodeType::CatchAll;
            return parent.catch_all_.get();
        }
        return parent.catch_all_.get();
    }

    auto child = std::make_unique<Node>();
    child->type_ = type;

    Node* result = child.get();
    parent.static_.push_back(std::move(child));

    return result;
}

void Router::insert_route(
    http::verb method,
    const std::string& pattern,
    const Handler& handler
) {
    // Insert the static part of the pattern into the radix tree
    if (!roots_[method]) {
        roots_[method] = std::make_unique<Node>();
    }
    Node* current = roots_[method].get();
    std::vector<std::string> parameter_names;
    std::string_view remaining_pattern = pattern;
    while (!remaining_pattern.empty()) {
        // Find the next parameter or catch-all.
        const auto marker = remaining_pattern.find_first_of(":*");

        // No more parameters: insert the remaining literal text.
        if (marker == std::string_view::npos) {
            current = insert_static(*current, remaining_pattern);
            break;
        }

        // Insert the literal text preceding the parameter.
        if (marker != 0) {
            current = insert_static(
                *current,
                remaining_pattern.substr(0, marker)
            );
            remaining_pattern.remove_prefix(marker);
        }

        const bool catch_all = remaining_pattern.front() == '*';

        auto end = catch_all
            ? remaining_pattern.size()
            : remaining_pattern.find('/');

        if (end == std::string_view::npos) {
            end = remaining_pattern.size();
        }

        // Copy the name into this registration's local list.
        parameter_names.emplace_back(
            remaining_pattern.substr(1, end - 1)
        );

        current = insert_special(
            *current,
            catch_all
                ? NodeType::CatchAll
                : NodeType::Parameter
        );

        // Keep the following '/' for static insertion.
        remaining_pattern.remove_prefix(end);
    }

    if (current->route_) {
        throw std::invalid_argument("Duplicate route");
    }

    current->route_ = Route{
        std::cref(handler),
        std::move(parameter_names)
    };
}

}  // namespace VeloxServ