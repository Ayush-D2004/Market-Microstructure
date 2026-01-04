cmake_minimum_required(VERSION 3.15)
project(MarketMicrostructure)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")

# Source files
set(SOURCES
    main.cpp
    order_book/OrderBook.cpp
    io/EventReader.cpp
    strategy/Strategy.cpp
    metrics/Metrics.cpp
)

# Create executable
add_executable(market_engine ${SOURCES})

# Include directories
target_include_directories(market_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries (if needed)
# target_link_libraries(market_engine pthread)

# Installation
install(TARGETS market_engine DESTINATION bin)

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
