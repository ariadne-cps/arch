cmake_minimum_required(VERSION 3.15)
project(AriadneArchExamples LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Compile Ariadne and ARCH in one CMake build tree; no external installation.
option(BUILD_PYTHON_BINDINGS "Build optional Ariadne Python bindings" OFF)
add_subdirectory(ss/ard)
add_subdirectory(suite/nln)
