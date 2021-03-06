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
set(GMOCK_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/third_party/gmock)
include_directories(${GMOCK_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/src)

# ---[ Dependencies
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
if(PROTOBUF_FOUND)
   message(STATUS "protobuf library found")
else()
    message(FATAL_ERROR "protobuf library is needed but cant be found")
endif()
find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
endif()

# --[ Output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---[ Subdirectories
add_subdirectory(test)
add_subdirectory(src)
add_subdirectory(kvraft)

#message(STATUS "CMAKE_CXX_FLAGS=" "${CMAKE_CXX_FLAGS}")

#message(STATUS "PROJECT_BINARY_DIR=" "${PROJECT_BINARY_DIR}")

#message(STATUS "PROJECT_SOURCE_DIR=" "${PROJECT_SOURCE_DIR}")

#message(STATUS "CMAKE_BINARY_DIR=" "${CMAKE_BINARY_DIR}")
