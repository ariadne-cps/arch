# Also supports direct configuration: cmake -S nln/suite -B build-suite.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.15)
    project(AriadneArchSuite LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    find_package(Ariadne REQUIRED)
endif()

# Compile the experimental suite, but never run it from CI.
set(ARCH_EXAMPLES
    ROBE25
    CVDP23
    LALO20
    LOVO25
    SPRE22
    SUTR21
    arch_suite
)
foreach(example IN LISTS ARCH_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE Ariadne::ariadne)
endforeach()

# QUAD20.cpp is preserved but not buildable: QUAD20.hpp is missing upstream.
