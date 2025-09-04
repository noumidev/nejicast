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
    src/scheduler.cpp
    src/common/file.cpp
    src/hw/cpu/cpu.cpp
    src/hw/cpu/ocio.cpp
    src/hw/g1/g1.cpp
    src/hw/g1/gdrom.cpp
    src/hw/g2/aica.cpp
    src/hw/g2/g2.cpp
    src/hw/g2/modem.cpp
    src/hw/holly/bus.cpp
    src/hw/holly/holly.cpp
    src/hw/holly/intc.cpp
    src/hw/holly/maple.cpp
    src/hw/pvr/core.cpp
    src/hw/pvr/interface.cpp
    src/hw/pvr/spg.cpp
    src/hw/pvr/ta.cpp
)

# Set header files
set(HEADERS
    include/nejicast.hpp
    include/scheduler.hpp
    include/common/config.hpp
    include/common/file.hpp
    include/common/types.hpp
    include/hw/cpu/cpu.hpp
    include/hw/cpu/ocio.hpp
    include/hw/g1/g1.hpp
    include/hw/g1/gdrom.hpp
    include/hw/g2/aica.hpp
    include/hw/g2/g2.hpp
    include/hw/g2/modem.hpp
    include/hw/holly/bus.hpp
    include/hw/holly/holly.hpp
    include/hw/holly/intc.hpp
    include/hw/holly/maple.hpp
    include/hw/pvr/core.hpp
    include/hw/pvr/interface.hpp
    include/hw/pvr/spg.hpp
    include/hw/pvr/ta.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
