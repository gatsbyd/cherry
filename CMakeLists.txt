cmake_minimum_required(VERSION 2.8)

project(cherry CXX C)

set(CXX_FLAGS
 -g
 -pg
 -Wall
 -Wextra
 -Werror
 -std=c++11
 -pthread
)

# ---[ Flags
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

# ---[ Includes
include_directories(${PROJECT_SOURCE_DIR}/src)

# ---[ Dependencies
find_package(Threads REQUIRED)

# --[ Output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---[ Subdirectories
add_subdirectory(test)
add_subdirectory(src)

#message(STATUS "CMAKE_CXX_FLAGS=" "${CMAKE_CXX_FLAGS}")

#message(STATUS "PROJECT_BINARY_DIR=" "${PROJECT_BINARY_DIR}")

#message(STATUS "PROJECT_SOURCE_DIR=" "${PROJECT_SOURCE_DIR}")

#message(STATUS "CMAKE_BINARY_DIR=" "${CMAKE_BINARY_DIR}")