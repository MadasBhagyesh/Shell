cmake_minimum_required(VERSION 3.14)

project(shell-starter-cpp)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

add_executable(shell ${SOURCE_FILES}
        src/Shell.cpp
        src/Shell.h
        src/ShellBuiltins.cpp
)

set(REPROC++ ON)
add_subdirectory(external/reproc-main/)

target_link_libraries(shell PRIVATE readline)
target_link_libraries(shell PRIVATE reproc++)