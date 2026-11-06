cmake_minimum_required(VERSION 3.16)
project(fd2sdl C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# magic_fx 使用 sin/cos；引用 core 的 TARGET_OBJECTS 不传递链接依赖。
# 在项目目标创建前显式链接 libm，覆盖主程序及同目录的状态机测试。
if(UNIX)
    link_libraries(m)
endif()

if(MSVC)
    add_compile_options(/utf-8 /experimental:c11atomics /std:c11)
endif()

set(FD2SDL_CORE_SOURCES
    app_flow.c
    title.c
    input.c
    audio.c
    audio_pcm.c
    bgm.cpp
    third_party/nukedopl3.c
    third_party/model_ail.c
    field_audio.c
    field_effect.c
    field_command.c
    field_system_menu.c
    field_manual_slot.c
    dialog_flow.c
    archive.c
    image.c
    figani.c
    battle_animation.c
    magic_fx.c
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
    field_save.c
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
target_include_directories(fd2sdl_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(fd2sdl
    main.c
    field_play.c
    field_prepare_play.c
    $<TARGET_OBJECTS:fd2sdl_core>
)
target_link_libraries(fd2sdl PRIVATE ${FD2SDL_SDL3_TARGET})

include(CTest)
if(BUILD_TESTING)
    option(FD2SDL_REQUIRE_GUI_TESTS "图形回归依赖不全时配置失败，避免静默缺测" OFF)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME field_play_input_harness
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/field_play_input_test.py --self-test)
    endif()
    add_executable(fd2_resource_test
        tests/resource_test.c
        archive.c
        image.c
        shape.c
    )
    target_include_directories(fd2_resource_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(fd2_resource_test PRIVATE /W4)
    else()
        target_compile_options(fd2_resource_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME resource COMMAND fd2_resource_test)
    set_tests_properties(resource PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_app_flow_test
        tests/app_flow_test.c
        app_flow.c
    )
    target_include_directories(fd2_app_flow_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(fd2_app_flow_test PRIVATE /W4)
    else()
        target_compile_options(fd2_app_flow_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME app_flow COMMAND fd2_app_flow_test)

    add_executable(fd2_main_title_resource_test
        tests/main_title_resource_test.c
        field_play.c
        field_prepare_play.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_main_title_resource_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_main_title_resource_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_main_title_resource_test PRIVATE /W4)
    else()
        target_compile_options(fd2_main_title_resource_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME main_title_resource COMMAND fd2_main_title_resource_test)
    set_tests_properties(main_title_resource PROPERTIES TIMEOUT 30)

    add_executable(fd2_save_test
        tests/save_test.c
        save.c
    )
    target_include_directories(fd2_save_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(fd2_save_test PRIVATE /W4)
    else()
        target_compile_options(fd2_save_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME save COMMAND fd2_save_test)
    set_tests_properties(save PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_manual_slot_test
        tests/field_manual_slot_test.c
        field_manual_slot.c
        save.c
    )
    target_include_directories(fd2_field_manual_slot_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(fd2_field_manual_slot_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_manual_slot_test PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_manual_slot COMMAND fd2_field_manual_slot_test)


    add_executable(fd2_field_stage_tables_test
        tests/field_stage_tables_test.c field.c archive.c)
    target_include_directories(fd2_field_stage_tables_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(fd2_field_stage_tables_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_stage_tables_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_stage_tables COMMAND fd2_field_stage_tables_test)
    set_tests_properties(field_stage_tables PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

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

    add_executable(fd2_field_natural_plan
        tests/field_natural_plan.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_natural_plan PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_natural_plan PRIVATE ${FD2SDL_SDL3_TARGET})
    set_target_properties(fd2_field_natural_plan PROPERTIES LINKER_LANGUAGE CXX)
    if(MSVC)
        target_compile_options(fd2_field_natural_plan PRIVATE /W4)
    else()
        target_compile_options(fd2_field_natural_plan PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME natural_plan_simulation COMMAND fd2_field_natural_plan
        --save "${CMAKE_SOURCE_DIR}/../original_game/START.SAV" --simulate
        --seed 0x7a18 --max-turns 100)
    set_tests_properties(natural_plan_simulation PROPERTIES
        TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.." LABELS "planning")

    add_executable(fd2_field_natural_plan_test
        tests/field_natural_plan_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_natural_plan_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_natural_plan_test PRIVATE ${FD2SDL_SDL3_TARGET})
    set_target_properties(fd2_field_natural_plan_test PROPERTIES LINKER_LANGUAGE CXX)
    if(MSVC)
        target_compile_options(fd2_field_natural_plan_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_natural_plan_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME natural_plan COMMAND fd2_field_natural_plan_test)
    set_tests_properties(natural_plan PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.." LABELS "planning")

    add_executable(fd2_bgm_capture_probe
        tests/bgm_capture_probe.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_bgm_capture_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_bgm_capture_probe PRIVATE ${FD2SDL_SDL3_TARGET})
    set_target_properties(fd2_bgm_capture_probe PROPERTIES LINKER_LANGUAGE CXX)
    if(MSVC)
        target_compile_options(fd2_bgm_capture_probe PRIVATE /W4)
    else()
        target_compile_options(fd2_bgm_capture_probe PRIVATE -Wall -Wextra -Wpedantic)
        target_link_libraries(fd2_bgm_capture_probe PRIVATE m)
    endif()

    add_executable(fd2_bgm_test
        tests/bgm_test.c
        bgm.cpp
        audio.c
        archive.c
        third_party/nukedopl3.c
        third_party/model_ail.c
    )
    target_include_directories(fd2_bgm_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_bgm_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_bgm_test PRIVATE /W4)
    else()
        target_compile_options(fd2_bgm_test PRIVATE -Wall -Wextra -Wpedantic)
        target_link_libraries(fd2_bgm_test PRIVATE m)
    endif()
    add_test(NAME bgm COMMAND fd2_bgm_test)
    set_tests_properties(bgm PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_title_test
        tests/title_test.c
        title.c
    )
    target_include_directories(fd2_title_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(fd2_title_test PRIVATE /W4)
    else()
        target_compile_options(fd2_title_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME title COMMAND fd2_title_test)

    add_executable(fd2_font_test
        tests/font_test.c
        font.c
        archive.c
    )
    target_include_directories(fd2_font_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_font_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_font_test PRIVATE /W4)
    else()
        target_compile_options(fd2_font_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME font COMMAND fd2_font_test)

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

    add_executable(fd2_dialog_flow_test
        tests/dialog_flow_test.c
        dialog_flow.c
    )
    target_include_directories(fd2_dialog_flow_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(fd2_dialog_flow_test PRIVATE /W4)
    else()
        target_compile_options(fd2_dialog_flow_test PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME dialog_flow COMMAND fd2_dialog_flow_test)

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
    if(MSVC)
        target_compile_options(fd2_field_command_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_command_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_command_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_command COMMAND fd2_field_command_test)

    add_executable(fd2_field_system_menu_test
        tests/field_system_menu_test.c
        field_system_menu.c
        field_command.c
        map_sprite.c
        image.c
        archive.c
    )
    target_include_directories(fd2_field_system_menu_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_system_menu_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_system_menu_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_system_menu_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_system_menu COMMAND fd2_field_system_menu_test)

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
        input.c
        archive.c
        shape.c
        image.c
        field.c
        tile.c
    )
    target_include_directories(fd2_field_visual_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_visual_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_visual_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_visual_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_visual COMMAND fd2_field_visual_test)

    add_executable(fd2_field_info_test
        tests/field_info_test.c
        field_info.c
        field_item.c
        field_magic.c
        field_unit_base.c
        field_unit_stats.c
        field_attack.c
        field_path.c
        field_move_profile.c
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
    if(MSVC)
        target_compile_options(fd2_field_info_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_info_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_info_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_info COMMAND fd2_field_info_test)
    set_tests_properties(field_info PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_combat_test
        tests/field_combat_test.c
        field_combat.c
    )
    target_include_directories(fd2_field_combat_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_combat_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_combat_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_combat COMMAND fd2_field_combat_test)

    add_executable(fd2_field_magic_test
        tests/field_magic_test.c
        field_magic.c
        field_unit_base.c
        field_unit_stats.c
        field_attack.c
        field_item.c
        field_path.c
        field_move_profile.c
    )
    target_include_directories(fd2_field_magic_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_magic_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_magic_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_magic_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_magic COMMAND fd2_field_magic_test)

    add_executable(fd2_field_magic_reward_test
        tests/field_magic_reward_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_magic_reward_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_magic_reward_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_magic_reward_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_magic_reward_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_magic_reward COMMAND fd2_field_magic_reward_test)

    add_executable(fd2_field_ai_test
        tests/field_ai_test.c
        field_ai.c
        field_attack.c
        field_path.c
        field_move_profile.c
        field_item.c
        field_magic.c
        field_unit_base.c
        field_unit_stats.c
    )
    target_include_directories(fd2_field_ai_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_ai_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_ai_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
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
    if(MSVC)
        target_compile_options(fd2_field_attack_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_attack_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
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
    if(MSVC)
        target_compile_options(fd2_field_rng_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_rng_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_rng COMMAND fd2_field_rng_test)

    add_executable(fd2_field_game_attack_test
        tests/field_game_attack_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_game_attack_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_game_attack_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_game_attack_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_game_attack_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_game_attack COMMAND fd2_field_game_attack_test)
    set_tests_properties(field_game_attack PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)

    add_executable(fd2_field_magic_flow_test
        tests/field_magic_flow_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_magic_flow_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_magic_flow_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_magic_flow_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_magic_flow_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_magic_flow COMMAND fd2_field_magic_flow_test)

    add_executable(fd2_field_magic_target_test
        tests/field_magic_target_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_magic_target_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_magic_target_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_magic_target_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_magic_target_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_magic_target COMMAND fd2_field_magic_target_test)

    add_executable(fd2_field_magic_commit_test
        tests/field_magic_commit_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_magic_commit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_magic_commit_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_magic_commit_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_magic_commit_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_magic_commit COMMAND fd2_field_magic_commit_test)
    set_tests_properties(field_magic_commit PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_game_failure_test
        tests/field_game_failure_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_game_failure_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_game_failure_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_game_failure_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_game_failure_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_game_failure COMMAND fd2_field_game_failure_test)

    add_executable(fd2_field_turn_test
        tests/field_turn_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_turn_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_turn_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_turn_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_turn_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_turn COMMAND fd2_field_turn_test)

    add_executable(fd2_field_player_controller_test
        tests/field_player_controller_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_player_controller_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_player_controller_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_player_controller_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_player_controller_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_player_controller COMMAND fd2_field_player_controller_test)

    add_executable(fd2_field_focus_test
        tests/field_focus_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_focus_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_focus_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_focus_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_focus_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_focus COMMAND fd2_field_focus_test)

    add_executable(fd2_field_march_test
        tests/field_march_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_march_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_march_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_march_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_march_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_march COMMAND fd2_field_march_test)
    set_tests_properties(field_march PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_water_test
        tests/field_water_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_water_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_water_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_water_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_water_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_water COMMAND fd2_field_water_test)
    set_tests_properties(field_water PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    # 私有anchor几何直接包含field_game.c；过滤重复对象，保留其余正式依赖。
    add_executable(fd2_field_geometry_test
        tests/field_geometry_test.c
        $<FILTER:$<TARGET_OBJECTS:fd2sdl_core>,EXCLUDE,field_game[.]>)
    target_include_directories(fd2_field_geometry_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_geometry_test PRIVATE ${FD2SDL_SDL3_TARGET})
    # core包含音乐C++对象；过滤后的对象列表仍须使用C++链接器。
    set_property(TARGET fd2_field_geometry_test PROPERTY LINKER_LANGUAGE CXX)
    if(MSVC)
        target_compile_options(fd2_field_geometry_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_geometry_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_geometry COMMAND fd2_field_geometry_test)
    set_tests_properties(field_geometry PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_unit_render_test
        tests/field_unit_render_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_unit_render_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_unit_render_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_unit_render_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_unit_render_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_unit_render COMMAND fd2_field_unit_render_test)
    set_tests_properties(field_unit_render PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)

    add_executable(fd2_field_game_system_menu_test
        tests/field_game_system_menu_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_game_system_menu_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_game_system_menu_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_game_system_menu_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_game_system_menu_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_game_system_menu
        COMMAND fd2_field_game_system_menu_test)

    add_executable(fd2_field_save_test
        tests/field_save_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_save_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_save_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_save_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_save_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_save COMMAND fd2_field_save_test)
    set_tests_properties(field_save PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_ai_commit_test
        tests/field_ai_commit_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_ai_commit_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_ai_commit_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_ai_commit_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
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
    if(MSVC)
        target_compile_options(fd2_field_unit_stats_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_unit_stats_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_unit_stats_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_unit_stats COMMAND fd2_field_unit_stats_test)

    add_executable(fd2_field_unit_base_test
        tests/field_unit_base_test.c
        field_unit_base.c
        field_item.c
        field_unit_stats.c
    )
    target_include_directories(fd2_field_unit_base_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_unit_base_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_unit_base_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_unit_base_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_unit_base COMMAND fd2_field_unit_base_test)

    add_executable(fd2_field_growth_test
        tests/field_growth_test.c
        field_unit_base.c
        field_item.c
        field_unit_stats.c
    )
    target_include_directories(fd2_field_growth_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(fd2_field_growth_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_growth_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(fd2_field_growth_test PRIVATE
        ${FD2SDL_SDL3_TARGET}
    )
    add_test(NAME field_growth COMMAND fd2_field_growth_test)

    add_executable(fd2_field_persistent_stats_test
        tests/field_persistent_stats_test.c
        field_unit_base.c field_item.c field_unit_stats.c)
    add_executable(fd2_field_stage_transition_test
        tests/field_stage_transition_test.c $<TARGET_OBJECTS:fd2sdl_core>)
    add_executable(fd2_field_loot_test
        tests/field_loot_test.c
        $<FILTER:$<TARGET_OBJECTS:fd2sdl_core>,EXCLUDE,field_game[.]>)
    set_property(TARGET fd2_field_stage_transition_test fd2_field_loot_test
        PROPERTY LINKER_LANGUAGE CXX)
    foreach(fd2_b5_target fd2_field_persistent_stats_test
            fd2_field_stage_transition_test fd2_field_loot_test)
        target_include_directories(${fd2_b5_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(${fd2_b5_target} PRIVATE ${FD2SDL_SDL3_TARGET})
        if(MSVC)
            target_compile_options(${fd2_b5_target} PRIVATE /W4)
        else()
            target_compile_options(${fd2_b5_target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
    add_test(NAME field_persistent_stats COMMAND fd2_field_persistent_stats_test)
    add_test(NAME field_stage_transition COMMAND fd2_field_stage_transition_test)
    add_test(NAME field_loot COMMAND fd2_field_loot_test)
    set_tests_properties(field_persistent_stats field_stage_transition field_loot
        PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    if(WIN32)
        if(NOT Python3_Interpreter_FOUND)
            if(FD2SDL_REQUIRE_GUI_TESTS)
                message(FATAL_ERROR "Windows GUI tests require Python 3")
            endif()
            message(WARNING "field_play_input not registered: install Python 3 and reconfigure")
        else()
            add_executable(fd2_field_play_input_driver
                tests/field_play_input_driver.c tests/window_capture.cpp)
            # C++/WinRT 只用于测试工具，音乐生产后端保持 C++14。
            set_target_properties(fd2_field_play_input_driver PROPERTIES CXX_STANDARD 17)
            target_link_libraries(fd2_field_play_input_driver PRIVATE
                user32 d3d11 dxgi windowsapp dwmapi windowscodecs ole32 shell32)
            add_executable(fd2_process_audio_capture tests/process_audio_capture_win.cpp)
            target_link_libraries(fd2_process_audio_capture PRIVATE mmdevapi ole32 uuid)
            if(MSVC)
                target_link_options(fd2_process_audio_capture PRIVATE
                    /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup)
            elseif(MINGW)
                target_link_options(fd2_process_audio_capture PRIVATE -mwindows -municode)
            endif()
            add_executable(fd2_process_audio_capture_test tests/process_audio_capture_test.cpp)
            add_executable(fd2_window_capture_series_test tests/window_capture_series_test.cpp)
            foreach(fd2_test IN ITEMS fd2_process_audio_capture fd2_process_audio_capture_test fd2_window_capture_series_test)
                set_target_properties(${fd2_test} PROPERTIES CXX_STANDARD 17)
                target_compile_definitions(${fd2_test} PRIVATE
                    _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A00000A)
                if(MSVC)
                    target_compile_options(${fd2_test} PRIVATE /W4)
                else()
                    target_compile_options(${fd2_test} PRIVATE -Wall -Wextra -Wpedantic)
                endif()
            endforeach()
            target_link_libraries(fd2_process_audio_capture_test PRIVATE mmdevapi ole32 uuid)
            target_link_libraries(fd2_window_capture_series_test PRIVATE
                user32 d3d11 dxgi windowsapp dwmapi windowscodecs ole32 shell32)
            add_test(NAME process_audio_capture COMMAND fd2_process_audio_capture_test)
            add_test(NAME window_capture_series COMMAND fd2_window_capture_series_test)
            set_tests_properties(process_audio_capture window_capture_series PROPERTIES
                TIMEOUT 30 LABELS "process;evidence")
            add_test(NAME graphics_job
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/field_play_job_test.py
                    --driver $<TARGET_FILE:fd2_field_play_input_driver>
                    --artifacts ${CMAKE_BINARY_DIR}/test-artifacts/graphics-job)
            set_tests_properties(graphics_job PROPERTIES
                TIMEOUT 60 LABELS "integration;process" RUN_SERIAL TRUE)
            add_test(NAME field_play_input
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/field_play_input_test.py
                    --driver $<TARGET_FILE:fd2_field_play_input_driver>
                    --game $<TARGET_FILE:fd2sdl>
                    --root ${CMAKE_SOURCE_DIR}/..
                    --artifacts ${CMAKE_BINARY_DIR}/test-artifacts/field-play-input)
            set_tests_properties(field_play_input PROPERTIES
                TIMEOUT 180 LABELS "e2e;graphics" RUN_SERIAL TRUE)
        endif()
    endif()

    # 仅该测试重新编译正式循环并替换依赖，兼容 MSVC，不给生产代码增加钩子。
    add_executable(fd2_field_play_result_test
        tests/field_play_result_test.c
        field_play.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_play_result_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_play_result_test PRIVATE ${FD2SDL_SDL3_TARGET})
    target_compile_definitions(fd2_field_play_result_test PRIVATE
        fd2_field_game_close=__wrap_fd2_field_game_close
        fd2_field_game_animate_command=__wrap_fd2_field_game_animate_command
        fd2_field_game_open=__wrap_fd2_field_game_open
        fd2_field_game_process_automatic_action_visual=__wrap_fd2_field_game_process_automatic_action_visual
        fd2_scene_play_field_event=__wrap_fd2_scene_play_field_event
        fd2_vga_present=__wrap_fd2_vga_present
        fd2_input_take_action=__wrap_fd2_input_take_action
        fd2_field_game_open_detail=__wrap_fd2_field_game_open_detail
        fd2_field_game_close_detail=__wrap_fd2_field_game_close_detail
        fd2_field_game_animate_detail=__wrap_fd2_field_game_animate_detail
        fd2_field_game_confirm_cursor=__wrap_fd2_field_game_confirm_cursor
        fd2_field_game_execute_move=__wrap_fd2_field_game_execute_move
        fd2_field_game_animate_system_menu=__wrap_fd2_field_game_animate_system_menu
        fd2_field_game_execute_storage_action=__wrap_fd2_field_game_execute_storage_action
        fd2_field_unit_summary_draw=__wrap_fd2_field_unit_summary_draw
        fd2_field_game_march=__wrap_fd2_field_game_march)
    add_test(NAME field_play_result COMMAND fd2_field_play_result_test)
    set_tests_properties(field_play_result PROPERTIES TIMEOUT 10)

    # 测试TU私有替身覆盖modal分配、呈现与输入，不给生产代码增加钩子。
    add_executable(fd2_field_play_overview_test
        tests/field_play_overview_test.c
        $<TARGET_OBJECTS:fd2sdl_core>
    )
    target_include_directories(fd2_field_play_overview_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_play_overview_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_play_overview_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_play_overview_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_play_overview COMMAND fd2_field_play_overview_test)
    set_tests_properties(field_play_overview PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_play_auxiliary_test
        tests/field_play_auxiliary_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_play_auxiliary_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_play_auxiliary_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_play_auxiliary_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_play_auxiliary_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_play_auxiliary COMMAND fd2_field_play_auxiliary_test)
    set_tests_properties(field_play_auxiliary PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_detail_flow_test
        tests/field_detail_flow_test.c
        $<FILTER:$<TARGET_OBJECTS:fd2sdl_core>,EXCLUDE,field_game[.]>)
    target_include_directories(fd2_field_detail_flow_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_detail_flow_test PRIVATE ${FD2SDL_SDL3_TARGET})
    set_property(TARGET fd2_field_detail_flow_test PROPERTY LINKER_LANGUAGE CXX)
    if(MSVC)
        target_compile_options(fd2_field_detail_flow_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_detail_flow_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_detail_flow COMMAND fd2_field_detail_flow_test)
    set_tests_properties(field_detail_flow PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_item_select_test
        tests/field_item_select_test.c
        field_item.c)
    target_include_directories(fd2_field_item_select_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_item_select_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_item_select_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_item_select_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_item_select COMMAND fd2_field_item_select_test)

    add_executable(fd2_field_item_flow_test
        tests/field_item_flow_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_item_flow_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_item_flow_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_item_flow_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_item_flow_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_item_flow COMMAND fd2_field_item_flow_test)

    add_executable(fd2_field_item_target_test
        tests/field_item_target_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_item_target_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_item_target_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_item_target_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_item_target_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_item_target COMMAND fd2_field_item_target_test)

    add_executable(fd2_field_item_commit_test
        tests/field_item_commit_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_item_commit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_item_commit_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_item_commit_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_item_commit_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_item_commit COMMAND fd2_field_item_commit_test)

    add_executable(fd2_field_item_menu_render_test
        tests/field_item_menu_render_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_item_menu_render_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_item_menu_render_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_item_menu_render_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_item_menu_render_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_item_menu_render COMMAND fd2_field_item_menu_render_test)

    add_executable(fd2_field_magic_list_play_test
        tests/field_magic_list_play_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_magic_list_play_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_magic_list_play_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_magic_list_play_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_magic_list_play_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_magic_list_play COMMAND fd2_field_magic_list_play_test)
    set_tests_properties(field_magic_list_play PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_prepare_play_test
        tests/field_prepare_play_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_prepare_play_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_prepare_play_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_prepare_play_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_prepare_play_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_prepare_play COMMAND fd2_field_prepare_play_test)
    set_tests_properties(field_prepare_play PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_progress_play_test
        tests/field_progress_play_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_progress_play_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_progress_play_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_progress_play_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_progress_play_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_progress_play COMMAND fd2_field_progress_play_test)
    set_tests_properties(field_progress_play PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_progress_test
        tests/field_progress_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_progress_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_progress_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_progress_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_progress_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_progress COMMAND fd2_field_progress_test)
    set_tests_properties(field_progress PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_field_system_message_test
        tests/field_system_message_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_field_system_message_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_field_system_message_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_field_system_message_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_system_message_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_system_message COMMAND fd2_field_system_message_test)
    set_tests_properties(field_system_message PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    add_executable(fd2_scene_handoff_test
        tests/scene_handoff_test.c
        $<TARGET_OBJECTS:fd2sdl_core>)
    target_include_directories(fd2_scene_handoff_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fd2_scene_handoff_test PRIVATE ${FD2SDL_SDL3_TARGET})
    if(MSVC)
        target_compile_options(fd2_scene_handoff_test PRIVATE /W4)
    else()
        target_compile_options(fd2_scene_handoff_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME scene_handoff COMMAND fd2_scene_handoff_test)
    add_test(NAME scene_transition COMMAND fd2_scene_handoff_test --b5)
    set_tests_properties(scene_transition PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")
    set_tests_properties(scene_handoff PROPERTIES
        TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

    if(UNIX AND NOT APPLE)
        # 依赖齐备时另注册 Xvfb/XTest 回归，发送真实按键而不调用 field_game API。
        find_package(X11 QUIET)
        find_program(FD2SDL_XVFB_EXECUTABLE Xvfb)
        if(X11_FOUND AND X11_Xtst_LIB AND FD2SDL_XVFB_EXECUTABLE)
            add_executable(fd2_field_play_input_driver
                tests/field_play_input_driver.c
            )
            target_include_directories(fd2_field_play_input_driver PRIVATE
                ${X11_X11_INCLUDE_PATH}
                ${X11_Xtst_INCLUDE_PATH}
            )
            target_link_libraries(fd2_field_play_input_driver PRIVATE
                ${X11_X11_LIB}
                ${X11_Xtst_LIB}
            )
            target_compile_options(fd2_field_play_input_driver PRIVATE
                -Wall -Wextra -Wpedantic)
            add_test(NAME field_play_input
                COMMAND sh
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/field_play_input_test.sh
                    $<TARGET_FILE:fd2_field_play_input_driver>
                    $<TARGET_FILE:fd2sdl>
                    ${CMAKE_SOURCE_DIR}/..
            )
            set_tests_properties(field_play_input PROPERTIES
                TIMEOUT 90
                LABELS "e2e;graphics"
                RUN_SERIAL TRUE
            )
        elseif(FD2SDL_REQUIRE_GUI_TESTS)
            message(FATAL_ERROR "Linux GUI tests require X11, XTest and Xvfb")
        else()
            message(WARNING "field_play_input not registered: install X11, XTest and Xvfb")
        endif()
    endif()

    add_executable(fd2_figani_test tests/figani_test.c figani.c image.c archive.c)
    add_executable(fd2_field_magic_full_test
        tests/field_magic_full_test.c $<TARGET_OBJECTS:fd2sdl_core>)
    add_executable(fd2_magic_fx_test tests/magic_fx_test.c magic_fx.c)
    add_executable(fd2_battle_animation_test
        tests/battle_animation_test.c
        figani.c magic_fx.c tile.c field_info.c field_visual.c field_item.c field_magic.c field_unit_base.c
        field_unit_stats.c field_attack.c field_path.c field_move_profile.c
        map_sprite.c image.c archive.c portrait.c text.c font.c field.c shape.c
        audio_pcm.c audio.c)
    foreach(fd2_test IN ITEMS fd2_figani_test fd2_magic_fx_test fd2_battle_animation_test fd2_field_magic_full_test)
        target_include_directories(${fd2_test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(${fd2_test} PRIVATE ${FD2SDL_SDL3_TARGET})
        if(MSVC)
            target_compile_options(${fd2_test} PRIVATE /W4)
        else()
            target_compile_options(${fd2_test} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
    add_test(NAME figani COMMAND fd2_figani_test)
    add_test(NAME magic_fx COMMAND fd2_magic_fx_test)
    add_test(NAME battle_animation COMMAND fd2_battle_animation_test)
    add_test(NAME field_magic_full COMMAND fd2_field_magic_full_test)
    set_tests_properties(figani magic_fx battle_animation field_magic_full PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.. TIMEOUT 30)

    # 测试中的 assert 包含实际调用；Release 也必须执行，不能得到空跑假绿。
    get_directory_property(fd2_targets BUILDSYSTEM_TARGETS)
    foreach(fd2_target IN LISTS fd2_targets)
        if(fd2_target MATCHES "^fd2_.*_test$")
            if(MSVC)
                target_compile_options(${fd2_target} PRIVATE /UNDEBUG)
            else()
                target_compile_options(${fd2_target} PRIVATE -UNDEBUG)
            endif()
        endif()
    endforeach()
endif()

# 让可执行文件能在源码树根目录运行并找到 original_game/
set_target_properties(fd2sdl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
