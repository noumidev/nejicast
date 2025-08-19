# Require CMake 3.16
cmake_minimum_required(VERSION 3.16)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Werror -Wextra)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

project(nejicast CXX)

# Set include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Set source files
set(SOURCES
    src/nejicast.cpp
)

# Set header files
set(HEADERS)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
