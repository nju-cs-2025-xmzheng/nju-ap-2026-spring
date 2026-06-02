cmake_minimum_required(VERSION 3.21)
project(Synera LANGUAGES CXX)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib REQUIRED)
find_package(Boost REQUIRED)
find_package(Threads REQUIRED)

if(TARGET Boost::headers)
    set(SYNERA_BOOST_TARGET Boost::headers)
elseif(TARGET Boost::boost)
    set(SYNERA_BOOST_TARGET Boost::boost)
endif()

add_executable(Synera
    src/main.cpp
    src/gui/GameApp.cpp
    src/gui/InputBox.cpp
)

target_include_directories(Synera PRIVATE src ${Boost_INCLUDE_DIRS})
target_link_libraries(Synera PRIVATE raylib Threads::Threads)
if(DEFINED SYNERA_BOOST_TARGET)
    target_link_libraries(Synera PRIVATE ${SYNERA_BOOST_TARGET})
endif()

include(CTest)

if(BUILD_TESTING)
    macro(add_synera_test name src_file)
        add_executable(${name} ${src_file})
        target_include_directories(${name} PRIVATE src ${Boost_INCLUDE_DIRS})
        target_link_libraries(${name} PRIVATE Threads::Threads)
        if(DEFINED SYNERA_BOOST_TARGET)
            target_link_libraries(${name} PRIVATE ${SYNERA_BOOST_TARGET})
        endif()
        add_test(NAME ${name} COMMAND $<TARGET_FILE:${name}>)
        set_tests_properties(${name} PROPERTIES LABELS "unit" TIMEOUT 5)
    endmacro()

    add_synera_test(test_battle test/test_battle.cpp)
    add_synera_test(test_gamemode test/test_gamemode.cpp)
    add_synera_test(test_economy test/test_economy.cpp)
    add_synera_test(test_coord test/test_coord.cpp)
    add_synera_test(test_board test/test_board.cpp)
    add_synera_test(test_equipment test/test_equipment.cpp)
    add_synera_test(test_synergy test/test_synergy.cpp)
    add_synera_test(test_unit test/test_unit.cpp)
    add_synera_test(test_serialization test/test_serialization.cpp)
    add_synera_test(test_network test/test_network.cpp)
    set_tests_properties(test_network PROPERTIES TIMEOUT 30)
    add_synera_test(test_mp_integration test/test_mp_integration.cpp)
    set_tests_properties(test_mp_integration PROPERTIES TIMEOUT 30)
endif()
