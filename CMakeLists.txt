cmake_minimum_required(VERSION 3.21)
project(Synera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)

add_executable(Synera
    src/main.cpp
)

target_include_directories(Synera PRIVATE src)
target_link_libraries(Synera PRIVATE SDL3::SDL3)
