cmake_minimum_required(VERSION 3.14)

project(DeadlockServerPicker
    VERSION 0.1.0
    DESCRIPTION "TUI tool to block/allow Deadlock server regions via the OS firewall"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

if(NOT FETCHCONTENT_SOURCE_DIR_FTXUI AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/FTXUI/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_FTXUI "${CMAKE_CURRENT_SOURCE_DIR}/deps/FTXUI")
    message(STATUS "Using local FTXUI from deps/FTXUI")
endif()

FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ftxui)

add_executable(deadlock-server-picker
    src/main.cpp
    src/firewall.cpp
    src/firewall.hpp
    src/ping.cpp
    src/ping.hpp
    src/process.cpp
    src/process.hpp
    src/servers.hpp
)

find_package(Threads REQUIRED)

target_link_libraries(deadlock-server-picker
    PRIVATE
        Threads::Threads
        ftxui::screen
        ftxui::dom
        ftxui::component
)

if(WIN32)
    target_link_libraries(deadlock-server-picker PRIVATE advapi32 shell32)
    target_compile_definitions(deadlock-server-picker PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(deadlock-server-picker PRIVATE /W4 /utf-8)
else()
    target_compile_options(deadlock-server-picker PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MINGW)
    target_link_options(deadlock-server-picker PRIVATE -static)
endif()
