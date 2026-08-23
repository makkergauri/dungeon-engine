cmake_minimum_required(VERSION 3.16)
project(dungeon-engine LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(DUNGEON_BUILD_TESTS "Build the unit test suite" ON)
option(DUNGEON_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Shared warning settings. Kept in an interface target so every target below
# picks them up identically instead of each one repeating the flags.
add_library(dungeon_warnings INTERFACE)
if(MSVC)
    target_compile_options(dungeon_warnings INTERFACE /W4 /permissive-)
    if(DUNGEON_WARNINGS_AS_ERRORS)
        target_compile_options(dungeon_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(dungeon_warnings INTERFACE -Wall -Wextra -Wpedantic)
    if(DUNGEON_WARNINGS_AS_ERRORS)
        target_compile_options(dungeon_warnings INTERFACE -Werror)
    endif()
endif()

# ------------------------------------------------------------------------------
# dungeon_core -- the half of the engine that has no graphics dependency
#
# ECS, collision maths, physics integration, timing, camera logic, pathfinding
# and dungeon generation. None of it includes an SFML header, which is what lets
# the test suite link against real production code without needing a window, a
# GPU or an audio device on the build machine. Keeping this boundary honest is
# the practical payoff of the "systems operate on plain data" design.
# ------------------------------------------------------------------------------
add_library(dungeon_core STATIC
    engine/core/Time.cpp
    engine/physics/Collision.cpp
    engine/physics/Physics.cpp
    engine/rendering/Camera.cpp
    game/ai/Pathfinding.cpp
    game/generation/DungeonGenerator.cpp
)
target_include_directories(dungeon_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dungeon_core PRIVATE dungeon_warnings)

# ------------------------------------------------------------------------------
# SFML
#
# 2.6.x specifically: SFML 3 renamed a good deal of the API (sf::Quads was
# removed, event handling moved to std::variant) and this codebase targets 2.
#
# Note that vcpkg's `sfml` port now installs 3.x, so `vcpkg install sfml` will
# NOT satisfy this. Use the official 2.6 binaries from sfml-dev.org and pass
# -DCMAKE_PREFIX_PATH=/path/to/SFML-2.6.2 instead.
# ------------------------------------------------------------------------------
find_package(SFML 2.6 COMPONENTS graphics window audio system QUIET)

if(NOT SFML_FOUND)
    find_package(SFML 2 COMPONENTS graphics window audio system QUIET)
endif()

if(NOT SFML_FOUND)
    message(WARNING
        "SFML 2.6 was not found. The core library and tests will still build, "
        "but the game executable will be skipped. Install SFML (see README.md) "
        "or pass -DCMAKE_PREFIX_PATH=/path/to/SFML to configure it.")
else()
    # --------------------------------------------------------------------------
    # dungeon_engine -- the rendering, windowing, input and audio layer
    # --------------------------------------------------------------------------
    add_library(dungeon_engine STATIC
        engine/core/Game.cpp
        engine/core/Window.cpp
        engine/rendering/Renderer.cpp
        engine/rendering/Sprite.cpp
        engine/rendering/ParticleSystem.cpp
        engine/input/InputManager.cpp
        engine/audio/AudioManager.cpp
    )
    target_link_libraries(dungeon_engine
        PUBLIC dungeon_core sfml-graphics sfml-window sfml-audio sfml-system
        PRIVATE dungeon_warnings
    )

    # --------------------------------------------------------------------------
    # dungeon -- the game
    # --------------------------------------------------------------------------
    add_executable(dungeon
        game/Main.cpp
        game/DungeonGame.cpp
        game/GameCommon.cpp
        game/ProceduralArt.cpp
        game/entities/Player.cpp
        game/entities/Enemy.cpp
        game/entities/Item.cpp
        game/combat/CombatSystem.cpp
        game/shop/Shop.cpp
        game/ui/Hud.cpp
        game/ProceduralArt.cpp
        game/ProceduralAudio.cpp
    )
    target_link_libraries(dungeon PRIVATE dungeon_engine dungeon_warnings)

    # Assets are loaded from paths relative to the working directory, so put a
    # copy next to the binary. A symlink would be lighter but breaks on Windows
    # without developer mode enabled.
    add_custom_command(TARGET dungeon POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:dungeon>/assets
        COMMENT "Copying assets next to the executable"
    )
endif()

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------
if(DUNGEON_BUILD_TESTS)
    enable_testing()

    # Prefer a system Catch2 when one is installed; otherwise fetch it. Trying
    # the local copy first keeps offline and CI-cached builds fast.
    find_package(Catch2 3 QUIET)

    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.2
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(dungeon_tests
        tests/test_collision.cpp
        tests/test_pathfinding.cpp
        tests/test_generation.cpp
        tests/test_ecs.cpp
    )
    # Note the absence of SFML here: every test links dungeon_core only.
    target_link_libraries(dungeon_tests PRIVATE dungeon_core Catch2::Catch2WithMain dungeon_warnings)

    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch OPTIONAL RESULT_VARIABLE CATCH_MODULE)
    if(CATCH_MODULE)
        catch_discover_tests(dungeon_tests)
    else()
        add_test(NAME dungeon_tests COMMAND dungeon_tests)
    endif()
endif()