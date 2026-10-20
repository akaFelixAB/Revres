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

#include "config/ConfigLoader.hpp"

#include <memory>

namespace spdlog {
class logger;  // NOLINT
}

namespace Revres {

class Logger final {
public:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void initialize_bootstrap();
    void initialize(const ServerConfig& config);
    void shutdown();

    [[nodiscard]] bool is_initialized() const noexcept {
        return initialized_;
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
    bool initialized_ = false;
};

}  // namespace Revres