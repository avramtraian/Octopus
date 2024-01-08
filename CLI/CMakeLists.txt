# Copyright (c) 2023 Traian Avram. All rights reserved.
# SPDX-License-Identifier: MIT.

set(OCTOPUS_CLI_SOURCE_FILES
        Bitmap.cpp
        Bitmap.h
        Command.cpp
        Command.h
        DatabaseCommands.cpp
        Font.cpp
        Font.h
        Main.cpp
        Print.cpp
        Print.h
        TicketWriteCommands.cpp
)

#
# Create the Octopus CLI executable program.
#
add_executable(Octopus-CLI ${OCTOPUS_CLI_SOURCE_FILES})
set_target_properties(Octopus-CLI PROPERTIES OUTPUT_NAME "oct")
target_include_directories(Octopus-CLI PRIVATE "${CMAKE_SOURCE_DIR}/CLI")
target_link_directories(Octopus-CLI PRIVATE "${CMAKE_SOURCE_DIR}/ThirdParty")

#
# Link against the Octopus core library.
#
add_dependencies(Octopus-CLI Octopus-Core)
target_link_libraries(Octopus-CLI PUBLIC "Octopus-Core")
target_include_directories(Octopus-CLI PUBLIC "${CMAKE_SOURCE_DIR}/Core")

#
# Link against libpng
#
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(Octopus-CLI PRIVATE "libpng16_staticd.lib")
    target_link_libraries(Octopus-CLI PRIVATE "zlibstaticd.lib")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_libraries(Octopus-CLI PRIVATE "libpng16_static.lib")
    target_link_libraries(Octopus-CLI PRIVATE "zlibstatic.lib")
else ()
    message("Specify CMAKE_BUILD_TYPE=<Debug/Release>!")
endif ()
