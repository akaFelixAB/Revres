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

#include <stdexcept>
#include <string>
#include <vector>
#include "ConfigDiagnostic.hpp"

namespace Revres {

class ConfigException final : public std::runtime_error {
public:
    explicit ConfigException(std::vector<ConfigDiagnostic> diagnostics);

    [[nodiscard]] const std::vector<ConfigDiagnostic>& diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    static std::string format_message(
        const std::vector<ConfigDiagnostic>& diagnostics
    );

    std::vector<ConfigDiagnostic> diagnostics_;
};

}  // namespace Revres
