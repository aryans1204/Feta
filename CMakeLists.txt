cmake_minimum_required(VERSION 3.25)
project(Pascal)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Builds tests" ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_executable(pascal src/main.cpp)

target_include_directories(pascal INTERFACE "include/")

add_subdirectory(src/market_data)
add_subdirectory(src/net)

include(FetchContent)

if(BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE 
    )
    FetchContent_MakeAvailable(Catch2)
    enable_testing()
    
    include(CTest)

    message(STATUS "CATCH2_SOURCE_DIR: ${catch2_SOURCE_DIR}")
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    add_executable(unit_tests 
        tests/unit/test_fix_order_book.cpp
        tests/unit/test_fix_parser.cpp
        tests/unit/test_fix_engine.cpp
    )
    find_package(QuickFIX REQUIRED)
    target_include_directories(unit_tests INTERFACE "include/")
    target_link_libraries(unit_tests PRIVATE 
        orderbooklib
        netlib
        Catch2::Catch2WithMain
        QuickFIX::QuickFIX
    )
    catch_discover_tests(unit_tests)
endif()

target_link_libraries(pascal PUBLIC 
    orderbooklib
    netlib
)


