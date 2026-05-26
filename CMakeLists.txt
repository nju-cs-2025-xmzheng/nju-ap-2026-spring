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

include(CTest)

if(BUILD_TESTING)
    macro(add_synera_test name src_file)
        add_executable(${name} ${src_file})
        target_include_directories(${name} PRIVATE src)
        target_link_libraries(${name} PRIVATE SDL3::SDL3)
        add_test(NAME ${name} COMMAND $<TARGET_FILE:${name}>)
        set_tests_properties(${name} PROPERTIES LABELS "unit" TIMEOUT 5)
    endmacro()

    add_synera_test(test_battle test/test_battle.cpp)
    add_synera_test(test_economy test/test_economy.cpp)
    add_synera_test(test_coord test/test_coord.cpp)
    add_synera_test(test_board test/test_board.cpp)
    add_synera_test(test_equipment test/test_equipment.cpp)
    add_synera_test(test_synergy test/test_synergy.cpp)
    add_synera_test(test_unit test/test_unit.cpp)
endif()