cmake_minimum_required(VERSION 3.16)
project(fd2sdl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(FD2SDL_USE_SYSTEM_SDL3 "优先使用系统安装的 SDL3" ON)
set(FD2SDL_SDL3_VERSION "3.2.12" CACHE STRING "CPM 回退下载的 SDL3 版本")
set(FD2SDL_SDL3_GIT_TAG "release-${FD2SDL_SDL3_VERSION}" CACHE STRING "CPM 回退下载的 SDL3 Git tag")
set(FD2SDL_CPM_VERSION "0.40.8" CACHE STRING "自动下载的 CPM.cmake 版本")

# SDL3 获取策略：先找系统包（CMake config，其次 pkg-config），找不到再用 CPM.cmake 拉取源码。
set(FD2SDL_SDL3_TARGET "")
if(FD2SDL_USE_SYSTEM_SDL3)
    find_package(SDL3 CONFIG QUIET)
    if(TARGET SDL3::SDL3)
        set(FD2SDL_SDL3_TARGET SDL3::SDL3)
    elseif(TARGET SDL3::SDL3-static)
        set(FD2SDL_SDL3_TARGET SDL3::SDL3-static)
    elseif(TARGET SDL3::SDL3-shared)
        set(FD2SDL_SDL3_TARGET SDL3::SDL3-shared)
    endif()

    if(NOT FD2SDL_SDL3_TARGET)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL3_PKG QUIET IMPORTED_TARGET sdl3)
            if(TARGET PkgConfig::SDL3_PKG)
                set(FD2SDL_SDL3_TARGET PkgConfig::SDL3_PKG)
            endif()
        endif()
    endif()
endif()

function(fd2sdl_include_cpm)
    set(CPM_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/cmake")
    set(CPM_DOWNLOAD_LOCATION "${CPM_DOWNLOAD_DIR}/CPM_${FD2SDL_CPM_VERSION}.cmake")
    if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
        file(MAKE_DIRECTORY "${CPM_DOWNLOAD_DIR}")
        message(STATUS "Downloading CPM.cmake ${FD2SDL_CPM_VERSION}")
        file(DOWNLOAD
            "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${FD2SDL_CPM_VERSION}/CPM.cmake"
            "${CPM_DOWNLOAD_LOCATION}"
            STATUS cpm_download_status
            TLS_VERIFY ON
        )
        list(GET cpm_download_status 0 cpm_status_code)
        list(GET cpm_download_status 1 cpm_status_message)
        if(NOT cpm_status_code EQUAL 0)
            file(REMOVE "${CPM_DOWNLOAD_LOCATION}")
            message(FATAL_ERROR
                "SDL3 not found locally, and downloading CPM.cmake failed: ${cpm_status_message}. "
                "Install SDL3, set SDL3_DIR, or retry with network access.")
        endif()
    endif()

    include("${CPM_DOWNLOAD_LOCATION}")
endfunction()

if(NOT FD2SDL_SDL3_TARGET)
    message(STATUS "SDL3 not found in system paths; fetching SDL3 ${FD2SDL_SDL3_GIT_TAG} via CPM.cmake")
    fd2sdl_include_cpm()

    # 回退构建只给本项目使用：关闭 SDL 示例和测试，并优先静态链接，避免运行时还要找动态库。
    set(SDL_SHARED OFF CACHE BOOL "Build SDL3 shared library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL3 static library" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL3 test library" FORCE)

    CPMAddPackage(
        NAME SDL3
        GITHUB_REPOSITORY libsdl-org/SDL
        GIT_TAG ${FD2SDL_SDL3_GIT_TAG}
    )

    if(TARGET SDL3::SDL3)
        set(FD2SDL_SDL3_TARGET SDL3::SDL3)
    elseif(TARGET SDL3::SDL3-static)
        set(FD2SDL_SDL3_TARGET SDL3::SDL3-static)
    elseif(TARGET SDL3::SDL3-shared)
        set(FD2SDL_SDL3_TARGET SDL3::SDL3-shared)
    elseif(TARGET SDL3-static)
        set(FD2SDL_SDL3_TARGET SDL3-static)
    elseif(TARGET SDL3-shared)
        set(FD2SDL_SDL3_TARGET SDL3-shared)
    endif()
endif()

if(NOT FD2SDL_SDL3_TARGET)
    message(FATAL_ERROR "SDL3 target was not created by system search or CPM.cmake fallback.")
endif()

set(FD2SDL_CORE_SOURCES
    input.c
    audio.c
    audio_pcm.c
    field_audio.c
    field_effect.c
    field_command.c
    archive.c
    image.c
    shape.c
    field.c
    field_unit.c
    field_unit_base.c
    field_path.c
    field_move.c
    field_move_profile.c
    field_attack.c
    field_ai.c
    field_combat.c
    field_rng.c
    field_item.c
    field_magic.c
    field_unit_stats.c
    field_event.c
    field_visual.c
    field_info.c
    field_game.c
    field_play.c
    field_preview.c
    text.c
    tile.c
    font.c
    portrait.c
    map_sprite.c
    save.c
    scene.c
    animation.c
    vga.c
    palette.c
)

# 主程序与依赖正式 session 的状态机测试共享同一组实现对象，避免测试
# 意外遗漏 field_game 的间接资源依赖。
add_library(fd2sdl_core OBJECT ${FD2SDL_CORE_SOURCES})
target_link_libraries(fd2sdl_core PUBLIC ${FD2SDL_SDL3_TARGET})

add_executable(fd2sdl
    main.c
    $<TARGET_OBJECTS:fd2sdl_core>
)
target_link_libraries(fd2sdl PRIVATE ${FD2SDL_SDL3_TARGET})

include(CTest)
if(BUILD_TESTING)
    add_executable(fd2_audio_test
        tests/audio_test.c
        audio.c
    )
    target_include_directories(fd2_audio_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_audio_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_audio_test PRIVATE /W4)
    else()
        target_compile_options(fd2_audio_test PRIVATE -Wall -Wextra -Wpedantic)
        target_link_libraries(fd2_audio_test PRIVATE m)
    endif()
    add_test(NAME audio COMMAND fd2_audio_test)
    set_tests_properties(audio PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    add_executable(fd2_input_test
        tests/input_test.c
        input.c
    )
    target_include_directories(fd2_input_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_input_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_input_test PRIVATE /W4)
    else()
        target_compile_options(fd2_input_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME input COMMAND fd2_input_test)

    add_executable(fd2_audio_pcm_test
        tests/audio_pcm_test.c
        audio_pcm.c
        audio.c
        field_audio.c
        archive.c
    )
    target_include_directories(fd2_audio_pcm_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_audio_pcm_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_audio_pcm_test PRIVATE /W4)
    else()
        target_compile_options(fd2_audio_pcm_test PRIVATE -Wall -Wextra -Wpedantic)
        target_link_libraries(fd2_audio_pcm_test PRIVATE m)
    endif()
    add_test(NAME audio_pcm COMMAND fd2_audio_pcm_test)

    add_executable(fd2_field_effect_test
        tests/field_effect_test.c
        field_effect.c
    )
    target_include_directories(fd2_field_effect_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_effect_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_effect_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_effect_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_effect COMMAND fd2_field_effect_test)

    add_executable(fd2_field_command_test
        tests/field_command_test.c
        field_command.c
        map_sprite.c
        image.c
        archive.c
    )
    target_include_directories(fd2_field_command_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_command_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_link_libraries(fd2_field_command_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_command COMMAND fd2_field_command_test)

    add_executable(fd2_field_path_test
        tests/field_path_test.c
        field_path.c
        field_move.c
        field_move_profile.c
    )
    target_include_directories(fd2_field_path_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_path_test PRIVATE ${FD2SDL_SDL3_TARGET})
    add_test(NAME field_path COMMAND fd2_field_path_test)

    add_executable(fd2_field_event_test
        tests/field_event_test.c
        field_event.c
    )
    target_include_directories(fd2_field_event_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_event_test PRIVATE ${FD2SDL_SDL3_TARGET})
    add_test(NAME field_event COMMAND fd2_field_event_test)

    add_executable(fd2_field_visual_test
        tests/field_visual_test.c
        field_visual.c
        archive.c
        shape.c
        image.c
        field.c
        tile.c
    )
    target_include_directories(fd2_field_visual_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_visual_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_link_libraries(fd2_field_visual_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_visual COMMAND fd2_field_visual_test)

    add_executable(fd2_field_info_test
        tests/field_info_test.c
        field_info.c
        field_item.c
        map_sprite.c
        image.c
        archive.c
        portrait.c
        text.c
        font.c
    )
    target_include_directories(fd2_field_info_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_info_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_link_libraries(fd2_field_info_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_info COMMAND fd2_field_info_test)

    add_executable(fd2_field_combat_test
        tests/field_combat_test.c
        field_combat.c
    )
    target_include_directories(fd2_field_combat_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_combat_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    add_test(NAME field_combat COMMAND fd2_field_combat_test)

    add_executable(fd2_field_ai_test
        tests/field_ai_test.c
        field_ai.c
        field_attack.c
        field_path.c
        field_move_profile.c
        field_item.c
        field_magic.c
    )
    target_include_directories(fd2_field_ai_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_ai_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_link_libraries(fd2_field_ai_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_ai COMMAND fd2_field_ai_test)

    add_executable(fd2_field_attack_test
        tests/field_attack_test.c
        field_attack.c
        field_item.c
        field_move_profile.c
        field_path.c
    )
    target_include_directories(fd2_field_attack_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_attack_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_link_libraries(fd2_field_attack_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_attack COMMAND fd2_field_attack_test)

    add_executable(fd2_field_rng_test
        tests/field_rng_test.c
        field_rng.c
    )
    target_include_directories(fd2_field_rng_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_rng_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    add_test(NAME field_rng COMMAND fd2_field_rng_test)

    add_executable(fd2_field_game_attack_test
        tests/field_game_attack_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_game_attack_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_game_attack_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_link_libraries(fd2_field_game_attack_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_game_attack COMMAND fd2_field_game_attack_test)

    add_executable(fd2_field_ai_commit_test
        tests/field_ai_commit_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_ai_commit_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_ai_commit_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_link_libraries(fd2_field_ai_commit_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_ai_commit COMMAND fd2_field_ai_commit_test)

    add_executable(fd2_field_unit_stats_test
        tests/field_unit_stats_test.c
        field_unit_stats.c
    )
    target_include_directories(fd2_field_unit_stats_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_unit_stats_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_link_libraries(fd2_field_unit_stats_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_unit_stats COMMAND fd2_field_unit_stats_test)

    add_executable(fd2_field_unit_base_test
        tests/field_unit_base_test.c
        field_unit_base.c
    )
    target_include_directories(fd2_field_unit_base_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(fd2_field_unit_base_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_link_libraries(fd2_field_unit_base_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_unit_base COMMAND fd2_field_unit_base_test)
endif()

# 让可执行文件能在源码树根目录运行并找到 original_game/
set_target_properties(fd2sdl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
