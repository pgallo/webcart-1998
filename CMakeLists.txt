cmake_minimum_required(VERSION 3.20)
project(webcart LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WEBCART_BUILD_TESTS "Build GoogleTest unit tests" ON)

if(MSVC)
  add_compile_options(/W4 /utf-8 "$<$<COMPILE_LANGUAGE:CXX>:/EHsc;/permissive->")
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS UNICODE _UNICODE)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Portable business tier: no platform headers, unit-tested on Linux.
add_library(webcart_core STATIC
  core/src/money.cpp
  core/src/catalog.cpp
  core/src/pricing.cpp
  core/src/stock.cpp
  core/src/journal.cpp
  core/src/shop.cpp
  core/src/xml.cpp
  core/src/gateway_protocol.cpp)
target_include_directories(webcart_core PUBLIC core/include)

add_executable(webcart-demo apps/demo/main.cpp)
target_link_libraries(webcart-demo PRIVATE webcart_core)

add_executable(webcart-gateway apps/gateway/main.cpp)
target_link_libraries(webcart-gateway PRIVATE webcart_core)
if(WIN32)
  target_link_libraries(webcart-gateway PRIVATE ws2_32)
endif()

if(WEBCART_BUILD_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()
  add_executable(webcart_tests
    tests/core/pricing_test.cpp
    tests/core/stock_test.cpp
    tests/core/journal_test.cpp
    tests/core/shop_test.cpp
    tests/core/gateway_protocol_test.cpp)
  target_link_libraries(webcart_tests PRIVATE webcart_core GTest::gtest GTest::gtest_main)
  target_compile_definitions(webcart_tests PRIVATE
    WEBCART_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
  include(GoogleTest)
  gtest_discover_tests(webcart_tests)
  add_test(NAME demo_runs COMMAND webcart-demo ${CMAKE_CURRENT_SOURCE_DIR}/data)
  set_tests_properties(demo_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "replayed 1 order\\(s\\), discarded 1 incomplete")
endif()

# COM local server, script and C++ client tests: Windows only.
if(WIN32)
  add_subdirectory(com)
endif()
