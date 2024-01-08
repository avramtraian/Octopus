# Copyright (c) 2023 Traian Avram. All rights reserved.
# SPDX-License-Identifier: MIT.

cmake_minimum_required(VERSION 3.25)
project(Octopus CXX C)

# Set the project global C++ configuration.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/BinDebug")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/BinDebug")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/BinDebug")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Bin")
else ()
    message("Specify CMAKE_BUILD_TYPE=<Debug/Release>")
endif ()

# Make the CMake predefined targets appear nicer if using the Visual Studio toolchain.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

#---------------------------------------------------------------
# Project subdirectories.
#---------------------------------------------------------------

# Contains the core logic and API that manipulates the database.
add_subdirectory(Core)

# Contains the command line application that is used to access the database.
add_subdirectory(CLI)
