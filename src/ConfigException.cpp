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

#include "ConfigException.hpp"

namespace Revres {

// Customized configuration validation errors
ConfigException::ConfigException(std::vector<ConfigDiagnostic> diagnostics)
    : std::runtime_error(format_message(diagnostics)),
      diagnostics_(std::move(diagnostics)) {}

std::string ConfigException::format_message(
        const std::vector<ConfigDiagnostic>& diagnostics
) {
    std::string message = "Configuration validation failed with the following errors:\n";

    for (const auto& diagnostic : diagnostics) {
        message += "  - Field: " + diagnostic.field_ + ", Message: " + diagnostic.message_ + "\n";
    }

    return message;
}

}  // namespace Revres
