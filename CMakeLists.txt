cmake_minimum_required(VERSION 3.21)

project(evm_project_2026 LANGUAGES C CXX)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

if(APPLE)
    find_program(HOMEBREW_EXECUTABLE brew)
    if(HOMEBREW_EXECUTABLE)
        execute_process(
            COMMAND "${HOMEBREW_EXECUTABLE}" --prefix raylib
            RESULT_VARIABLE RAYLIB_BREW_RESULT
            OUTPUT_VARIABLE RAYLIB_BREW_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(RAYLIB_BREW_RESULT EQUAL 0)
            list(PREPEND CMAKE_PREFIX_PATH "${RAYLIB_BREW_PREFIX}")
        endif()
    endif()
endif()

find_package(raylib 6.0 CONFIG QUIET)
if(NOT raylib_FOUND)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
    )
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(
    game-client
    graphics/raylib_basic_window.cpp
    graphics/client_network.cpp
    src/client/client_state.cpp
)
target_include_directories(game-client PRIVATE src src/utils third_party)
target_link_libraries(game-client PRIVATE raylib)

add_executable(
    game-server
    src/server/main.cpp
    src/server/net.cpp
    src/server/lobby.cpp
    src/server/game.cpp
)
target_include_directories(game-server PRIVATE src src/utils third_party)

if(WIN32)
    target_link_libraries(game-client PRIVATE ws2_32)
    target_link_libraries(game-server PRIVATE ws2_32)
endif()
