# Copyright (c) 2023 Traian Avram. All rights reserved.
# SPDX-License-Identifier: MIT.

set(OCTOPUS_CORE_SOURCE_FILES
        Core.h
        MathUtils.cpp
        MathUtils.h
        Result.h
        Table.h
        Table.cpp
)

add_library(Octopus-Core STATIC ${OCTOPUS_CORE_SOURCE_FILES})
target_include_directories(Octopus-Core PRIVATE "${CMAKE_SOURCE_DIR}/Core")
target_link_directories(Octopus-Core PRIVATE "${CMAKE_SOURCE_DIR}/ThirdParty")

#
# Link against yaml-cpp
#
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(Octopus-Core PRIVATE "yaml-cppd.lib")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_libraries(Octopus-Core PRIVATE "yaml-cpp.lib")
else ()
    message("Specify CMAKE_BUILD_TYPE=<Debug/Release>")
endif ()
