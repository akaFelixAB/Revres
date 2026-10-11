# Copyright 2026 Felix Huang

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 4.3.0)
project(VeloxServ VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Code Quality
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY 
        "${CLANG_TIDY_EXE};-header-filter=${CMAKE_SOURCE_DIR}/src/.*"
    )
endif()

# Deps
find_package(Boost REQUIRED COMPONENTS asio beast url)
find_package(tomlplusplus REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(Catch2 3 REQUIRED)

set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON) 

# Build Targets
add_library(VeloxServCore STATIC
    src/ConfigLoader.cpp
    src/ConfigValidator.cpp
    src/ConfigException.cpp
    src/ResponseFactory.cpp
    src/HttpServer.cpp
    src/HttpSession.cpp
    src/Logger.cpp
)

target_link_libraries(VeloxServCore PUBLIC 
    Boost::asio
    Boost::beast
    Boost::url
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

add_library(VeloxServ::Core ALIAS VeloxServCore)

target_include_directories(VeloxServCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(VeloxServ src/main.cpp)

target_link_libraries(VeloxServ PRIVATE
    VeloxServ::Core
)

# Testing
include(CTest)
enable_testing()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Smoke Test
add_test(
    NAME SmokeTest
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_test.py
        $<TARGET_FILE:VeloxServ>
        ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    SmokeTest
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        TIMEOUT 30
)

# Unit Tests
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")
add_executable(unit_tests ${TEST_SOURCES})

target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(unit_tests PRIVATE VeloxServCore)

include(Catch)
catch_discover_tests(unit_tests)

# if(WIN32)
#     # Windows 
#     install(TARGETS VeloxServ DESTINATION bin)
#     install(FILES default.toml DESTINATION bin)
# else()
#     # POSIX
#     install(TARGETS VeloxServ DESTINATION bin)
#     install(FILES default.toml DESTINATION /etc/VeloxServ)
# endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
