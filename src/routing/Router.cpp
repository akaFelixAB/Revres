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

#include <algorithm>
#include <spdlog/spdlog.h>
#include "boost/url.hpp"

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

// Build a string of allowed HTTP methods from the route map.
[[nodiscard]] std::string build_allowed_methods(
    const std::unordered_map<
        boost::beast::http::verb,
        std::shared_ptr<const Revres::Route>
    >& routes
) {
    std::vector<std::string_view> methods;
    methods.reserve(routes.size());

    for (const auto& [method, route] : routes) {
        methods.emplace_back(boost::beast::http::to_string(method));
    }

    std::sort(methods.begin(), methods.end());

    std::string result;
    for (const auto method : methods) {
        if (!result.empty()) {
            result += ", ";
        }
        result += method;
    }

    return result;
}

enum class RoutePatternError {
    None,
    InvalidPattern,
    InvalidParameterName,
    InvalidStaticSegment,
    InvalidDotSegment,
    CatchAllNotLast,
    MultipleCatchAll
};

[[nodiscard]] constexpr std::string_view to_string(RoutePatternError error) {
    switch (error) {
        case RoutePatternError::None:
            return "none";
        case RoutePatternError::InvalidPattern:
            return "invalid route pattern";
        case RoutePatternError::InvalidParameterName:
            return "invalid parameter name";
        case RoutePatternError::InvalidStaticSegment:
            return "invalid static path segment";
        case RoutePatternError::InvalidDotSegment:
            return "dot segments are not allowed";
        case RoutePatternError::CatchAllNotLast:
            return "catch-all must be the final segment";
        case RoutePatternError::MultipleCatchAll:
            return "multiple catch-all segments";
    }

    return "unknown route pattern error";
}

struct NormalizedRoutePattern {
    std::string encoded_pattern_;
    std::vector<std::string> parameter_names_;
    RoutePatternError error_ = RoutePatternError::None;
};

[[nodiscard]] NormalizedRoutePattern parse_route_pattern(
    std::string_view pattern
) {
    namespace urls = boost::urls;

    NormalizedRoutePattern result;

    if (pattern.empty() || pattern.front() != '/') {
        result.error_ = RoutePatternError::InvalidPattern;
        return result;
    }

    if (pattern == "/") {
        result.encoded_pattern_ = "/";
        return result;
    }

    if (pattern.back() == '/') {
        pattern.remove_suffix(1);
    }

    std::string normalized;
    normalized.reserve(pattern.size());

    std::size_t position = 0;

    while (position < pattern.size()) {
        const auto next = pattern.find('/', position + 1);

        const auto segment_end =
            next == std::string_view::npos
                ? pattern.size()
                : next;

        const auto segment = pattern.substr(
            position + 1,
            segment_end - position - 1
        );

        if (segment.empty()) {
            result.error_ = RoutePatternError::InvalidPattern;
            return result;
        }

        if (segment.front() == ':') {
            if (segment.size() == 1) {
                result.error_ =
                    RoutePatternError::InvalidParameterName;
                return result;
            }

            result.parameter_names_.emplace_back(
                segment.substr(1)
            );

            normalized += '/';
            normalized += segment;
        }
        else if (segment.front() == '*') {
            if (segment.size() == 1) {
                result.error_ =
                    RoutePatternError::InvalidParameterName;
                return result;
            }

            if (segment_end != pattern.size()) {
                result.error_ =
                    RoutePatternError::CatchAllNotLast;
                return result;
            }

            result.parameter_names_.emplace_back(
                segment.substr(1)
            );

            normalized += '/';
            normalized += segment;
            break;
        }
        else {
            if (segment == "." || segment == "..") {
                result.error_ =
                    RoutePatternError::InvalidDotSegment;
                return result;
            }

            std::string segment_input;
            segment_input.reserve(segment.size() + 1);
            segment_input += '/';
            segment_input += segment;

            const auto parsed = urls::parse_origin_form(
                boost::core::string_view(segment_input)
            );

            if (!parsed) {
                result.error_ =
                    RoutePatternError::InvalidStaticSegment;
                return result;
            }

            boost::urls::url url = parsed.value();
            url.normalize_path();

            const auto encoded = url.encoded_path();

            // Remove the synthetic leading '/'.
            const auto normalized_segment = encoded.substr(1);

            normalized += '/';
            normalized.append(
                normalized_segment.data(),
                normalized_segment.size()
            );
        }

        position = segment_end;
    }

    result.encoded_pattern_ = std::move(normalized);

    return result;
}

}

namespace Revres {

void Router::add_route(
    http::verb method,
    const std::string& pattern,
    const Handler& handler
) {
    if (frozen_.load(std::memory_order_acquire)) {
        throw std::logic_error(
            "Cannot add a route after the router has been frozen"
        );
    }

    const auto normalized = parse_route_pattern(pattern);

    if (normalized.error_ != RoutePatternError::None) {
        throw std::invalid_argument(
            "Invalid route pattern: " + pattern
        );
    }

    insert_route(method, normalized.encoded_pattern_, normalized.parameter_names_, handler);
}

void Router::freeze() noexcept {
    frozen_.store(true, std::memory_order_release);
}

std::optional<MatchResult> Router::match(
    http::verb method,
    std::string_view target
) const {
    target = target.substr(0, target.find('?'));  // Ignore query string for routing

    if (target.size() > 1 && target.back() == '/') {
        target.remove_suffix(1);
    }

    auto path_match = find_path(target);

    if (!path_match) {
        return std::nullopt;
    }

    return match_method(
        *path_match->node_,
        method,
        std::move(path_match->parameter_values_)
    );
}

std::optional<Router::PathMatch> Router::find_path(
    std::string_view target
) const {
    std::vector<std::string_view> parameter_values;

    // Traverse the radix tree to find a matching route
    auto search = [&](
        auto&& self,
        const Node* node,
        std::size_t position
    ) -> const Node* {
        // If we have consumed the entire target string and the current node has a route, we have a match.
        if (position == target.size() && !node->routes_.empty()) {
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
        if (node->catch_all_ && 
            !node->catch_all_->routes_.empty() &&
            !remaining.empty() && 
            remaining != "/"
        ) {
            // Catch-all matches the rest of the path.
            parameter_values.emplace_back(
                target.substr(position)
            );

            return node->catch_all_.get();
        }

        return nullptr;
    };

    const Node* matched = search(search, &roots_, 0);

    if (!matched) {
        return std::nullopt;
    }

    return PathMatch{
        matched,
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
            split_static_child(child, n);
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

void Router::split_static_child(
    Node& child,
    std::size_t prefix_length
) {
    auto suffix = std::make_unique<Node>();

    suffix->type_ = NodeType::Static;
    suffix->prefix_ = child.prefix_.substr(prefix_length);
    suffix->static_ = std::move(child.static_);
    suffix->parameter_ = std::move(child.parameter_);
    suffix->catch_all_ = std::move(child.catch_all_);
    suffix->routes_ = std::move(child.routes_);
    suffix->allowed_methods_ = std::move(child.allowed_methods_);

    child.prefix_.resize(prefix_length);
    child.static_.clear();
    child.routes_.clear();
    child.allowed_methods_.clear();

    child.static_.push_back(std::move(suffix));
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
    std::string_view pattern,
    std::vector<std::string> parameter_names,
    const Handler& handler
) {
    // Insert the static part of the pattern into the radix tree
    Node* current = &roots_;
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

        const auto segment_end = catch_all
            ? remaining_pattern.size()
            : remaining_pattern.find('/');

        const auto parameter_end =
            segment_end == std::string_view::npos
                ? remaining_pattern.size()
                : segment_end;

        current = insert_special(
            *current,
            catch_all
                ? NodeType::CatchAll
                : NodeType::Parameter
        );

        // Keep the following '/' for static insertion.
        remaining_pattern.remove_prefix(parameter_end);
    }

    if (current->routes_.contains(method)) {
        throw std::invalid_argument("Duplicate route");
    }

    current->routes_.emplace(
        method,
        std::make_shared<Route>(Route{
            handler,
            std::move(parameter_names)
        })
    );
    current->allowed_methods_ = build_allowed_methods(current->routes_);
}

std::optional<MatchResult> Router::match_method(
    const Node& node,
    http::verb method,
    std::vector<std::string_view>&& parameter_values
) const {
    const auto route_it = node.routes_.find(method);

    if (route_it == node.routes_.end()) {
        return MatchResult{
            nullptr,
            std::move(parameter_values),
            node.allowed_methods_
        };
    }

    return MatchResult{
        route_it->second,
        std::move(parameter_values),
        node.allowed_methods_
    };
}

}  // namespace Revres