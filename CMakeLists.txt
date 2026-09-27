cmake_minimum_required(VERSION 3.10)
project(ToolHub CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(CORE_SOURCES
    src/product.cpp
    src/supplier.cpp
    src/transaction_log.cpp
    src/inventory_manager.cpp
    src/reports.cpp
    src/file_handler.cpp
)

# Main application
add_executable(toolhub src/main.cpp ${CORE_SOURCES})

# Test executable (separate target, per the conception document's
# test approach: assert-based, no external framework)
add_executable(toolhub_tests tests/test_inventory.cpp ${CORE_SOURCES})
