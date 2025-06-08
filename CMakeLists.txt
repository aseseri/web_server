cmake_minimum_required(VERSION 3.10.0)
project(big-fat-dinosaur-w-short-front-legs)

# =============================================
# 1. Basic Project Configuration
# =============================================
# Make sure developers do not run cmake in the main project directory, to keep
# build artifacts from becoming clutter
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed.
    Please make a new directory (called a build directory) and run CMake from there.
    You may need to remove CMakeCache.txt." )
endif()

# Turn on debug builds if we are building in a devel shell
if (CMAKE_BUILD_TYPE STREQUAL "" AND "$ENV{DEVEL_SHELL}" STREQUAL "1")
  message(STATUS "Setting debug build type by default in devel shell")
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Output binaries to a sub directory "bin"
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================
# 2. Dependencies
# =============================================
# Enable GoogleTest
include(GoogleTest)
add_subdirectory(/usr/src/googletest googletest)

# Enable Boost
# Use static libraries so binaries can be deployed without a full boost install
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.50 REQUIRED COMPONENTS system log log_setup regex)
message(STATUS "Boost version: ${Boost_VERSION}")

# Enable Python (for integration tests)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

include_directories(include)

# =============================================
# 3. Main Targets
# =============================================
# Create libraries:
# Config Library
add_library(config_lib
    src/config.cc
    src/config_statement.cc
    src/config_parser.cc
) # reorganized config files into a single lib

# Network Library
add_library(network_lib
    src/server.cc
    src/session.cc
    src/server_runner.cc
    src/crud_handler.cc
    src/echo_handler.cc
    src/static_handler.cc
    src/mime_types.cc
    src/http_request.cc
    src/http_response.cc
    src/not_found_handler.cc
    src/not_found_handler_registry.cc
    src/registry.cc
    src/health_handler.cc
    src/sleep_handler.cc
    src/auth_handler.cc
    src/path_mutex_provider.cc
) # reorganized network files into a single lib

# Filesystem library
add_library(filesystem_lib
  src/filesystem.cc
)

target_link_libraries(config_lib
    Boost::log_setup
    Boost::log
)

# Link server_lib with session_lib since it depends on it
target_link_libraries(network_lib
    config_lib
    filesystem_lib
    Boost::system
    Boost::log
    Boost::log_setup
)

# Create main executable
add_executable(webserver src/_main.cc)
target_link_libraries(webserver
    network_lib
    Boost::log_setup
    Boost::log
    Boost::filesystem
    Boost::system
    Boost::thread
    pthread
)

# =============================================
# 4. Testing Configuration
# =============================================
# Enable cmake testing
include(CTest)
enable_testing()

# ----- Unit Tests -----
# Create test exe
add_executable(config_parser_test tests/config_parser_test.cc) #config parser tests
target_link_libraries(config_parser_test config_lib gtest_main)

add_executable(config_test tests/config_test.cc) #config tests
target_link_libraries(config_test config_lib gtest_main)

add_executable(server_test tests/server_test.cc) #server tests
target_link_libraries(server_test
    network_lib
    gtest_main
)

add_executable(session_test tests/session_test.cc) #session tests
target_link_libraries(session_test
    network_lib
    gtest_main
)
add_executable(server_runner_test tests/server_runner_test.cc)
target_link_libraries(server_runner_test
  network_lib
  config_lib
  gtest_main
)
add_executable(echo_handler_test tests/echo_handler_test.cc)
target_link_libraries(echo_handler_test
  network_lib
  config_lib
  gtest_main
)
add_executable(http_test tests/http_test.cc)
target_link_libraries(http_test
  network_lib
  config_lib
  gtest_main
)
add_executable(mime_types_test tests/mime_types_test.cc)
target_link_libraries(mime_types_test
  network_lib
  config_lib
  gtest_main
)
add_executable(static_handler_test tests/static_handler_test.cc)
target_link_libraries(static_handler_test
  network_lib
  config_lib
  gtest_main
)
add_executable(not_found_handler_test tests/not_found_handler_test.cc)
target_link_libraries(not_found_handler_test
  network_lib
  config_lib
  gtest_main
)
add_executable(registry_test tests/registry_test.cc)
target_link_libraries(registry_test
  network_lib
  config_lib
  gtest_main
)
add_executable(crud_handler_test tests/crud_handler_test.cc)
target_link_libraries(crud_handler_test
  network_lib
  config_lib
  gtest_main
  gmock_main
)
add_executable(health_handler_test tests/health_handler_test.cc)
target_link_libraries(health_handler_test
  network_lib
  config_lib
  gtest_main
)
add_executable(sleep_handler_test tests/sleep_handler_test.cc)
target_link_libraries(sleep_handler_test
  network_lib
  config_lib
  gtest_main
)
add_executable(auth_handler_test tests/auth_handler_test.cc)
target_link_libraries(auth_handler_test
  network_lib
  config_lib
  gtest_main
)
add_executable(path_mutex_provider_test tests/path_mutex_provider_test.cc)
target_link_libraries(path_mutex_provider_test
  network_lib
  config_lib
  gtest_main
)

# register tests
gtest_discover_tests(config_parser_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(config_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(server_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(session_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(server_runner_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(echo_handler_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(http_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(mime_types_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(static_handler_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(not_found_handler_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(registry_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(crud_handler_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(health_handler_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(sleep_handler_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(auth_handler_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
gtest_discover_tests(path_mutex_provider_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# ----- Integration Tests -----
if(Python3_Interpreter_FOUND)
  add_test(
        NAME integration_server_test
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_server_test.py
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
  set_tests_properties(integration_server_test PROPERTIES
        TIMEOUT 30
        LABELS "integration"
    )
else()
  message(WARNING "Python 3 interpreter not found - integration tests disabled")
endif()

# =============================================
# 5. Coverage Reporting
# =============================================
include(cmake/CodeCoverageReportConfig.cmake)
generate_coverage_report(
        TARGETS config_lib network_lib
        TESTS config_parser_test server_test session_test config_test server_runner_test echo_handler_test http_test mime_types_test static_handler_test not_found_handler_test registry_test crud_handler_test health_handler_test sleep_handler_test auth_handler_test path_mutex_provider_test
)

add_custom_target(reset
    COMMAND ${CMAKE_COMMAND} --build . --target clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Testing
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/Makefile
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/cmake_install.cmake
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CTestTestfile.cmake
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/DartConfiguration.tcl
    COMMAND ${CMAKE_COMMAND} ..
    COMMENT "Cleaned and reconfigured the build directory"
)
