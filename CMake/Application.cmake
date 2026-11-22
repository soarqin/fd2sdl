# 包括直接编译单个游戏模块的测试在内，共享同一个运行时设置实例。
add_library(fd2_settings STATIC src/settings.c src/bindings.c src/host_files.c)
target_link_libraries(fd2_settings PUBLIC ${FD2SDL_SDL3_TARGET})
target_include_directories(fd2_settings PUBLIC ${FD2_SRC})
link_libraries(fd2_settings)

# 单一游戏时间与音频调度；不链接DOS硬件研究。
add_library(fd2_clock STATIC src/bios_clock.c src/ail_timer.c)
target_include_directories(fd2_clock PUBLIC ${FD2_SRC})
target_link_libraries(fd2_clock PUBLIC ${FD2SDL_SDL3_TARGET})
link_libraries(fd2_clock)

# 仅保留影响实际PCM输出的采样、双声部混合和量化核。
add_library(fd2_audio_native STATIC src/audio_dig.c src/sb_dma_provider.c
    src/sb_channel.c src/audio_sb.c src/mixdown_dc.c)
target_include_directories(fd2_audio_native PUBLIC ${FD2_SRC})
target_link_libraries(fd2_audio_native PUBLIC fd2_clock)
if(MSVC)
    target_compile_options(fd2_audio_native PRIVATE /W4 /fp:strict)
else()
    target_compile_options(fd2_audio_native PRIVATE -Wall -Wextra -Wpedantic)
endif()
link_libraries(fd2_audio_native)

# 所有消费端（包括独立列源测试）链接同一份只读流程实例。
add_library(fd2_static_definitions STATIC src/static_data.c)
add_dependencies(fd2_static_definitions fd2_static_data)
target_include_directories(fd2_static_definitions PRIVATE ${FD2_SRC})
link_libraries(fd2_static_definitions)

set(FD2SDL_CORE_SOURCES
    src/app_flow.c
    src/title.c
    src/input.c
    src/audio.c
    src/audio_pcm_capture.c
    src/audio_pcm.c
    src/bgm.cpp
    src/third_party/nukedopl3.c
    src/third_party/model_ail.c
    src/field_audio.c
    src/field_effect.c
    src/field_defeat.c
    src/field_command.c
    src/field_system_menu.c
    src/field_manual_slot.c
    src/dialog_flow.c
    src/archive.c
    src/image.c
    src/figani.c
    src/battle_animation.c
    src/magic_fx.c
    src/shape.c
    src/field.c
    src/field_unit.c
    src/field_page.c
    src/field_background.c
    src/field_unit_base.c
    src/field_path.c
    src/field_move.c
    src/field_move_profile.c
    src/field_attack.c
    src/field_ai.c
    src/field_combat.c
    src/field_rng.c

    src/field_item.c
    src/field_prepare_item.c
    src/field_prepare.c
    src/field_prepare_view.c
    src/field_magic.c
    src/field_unit_stats.c
    src/field_event.c
    src/field_visual.c
    src/field_info.c
    src/field_round.c
    src/field_game.c
    src/field_save.c
    src/field_preview.c
    src/text.c
    src/tile.c
    src/font.c
    src/portrait.c
    src/map_sprite.c
    src/save.c
    src/scene.c
    src/scene_transition.c
    src/scene_teleport.c
    src/scene_cinematic.c
    src/animation.c
    src/vga.c
    src/palette.c
)

# 主程序与依赖正式 session 的状态机测试共享同一组实现对象，避免测试
# 意外遗漏 field_game 的间接资源依赖。
add_library(fd2sdl_core OBJECT ${FD2SDL_CORE_SOURCES})
add_dependencies(fd2sdl_core fd2_static_data)
target_link_libraries(fd2sdl_core PUBLIC ${FD2SDL_SDL3_TARGET})
target_include_directories(fd2sdl_core PRIVATE ${FD2_SRC})

add_executable(fd2sdl
    src/main.c
    src/field_play.c
    src/field_prepare_play.c
    $<TARGET_OBJECTS:fd2sdl_core>
)
target_link_libraries(fd2sdl PRIVATE ${FD2SDL_SDL3_TARGET})
add_dependencies(fd2sdl fd2_static_data)

# 发布工具读取实际CMake目标；不能从扩展名或旧SDK推断静态/动态链接。
if(WIN32)
    get_target_property(fd2_release_sdl_target ${FD2SDL_SDL3_TARGET} ALIASED_TARGET)
    if(NOT fd2_release_sdl_target)
        set(fd2_release_sdl_target ${FD2SDL_SDL3_TARGET})
    endif()
    get_target_property(fd2_release_sdl_type ${fd2_release_sdl_target} TYPE)
    file(TO_CMAKE_PATH "${SDL3_DIR}" fd2_release_sdl_package)
    if(fd2_release_sdl_type STREQUAL "STATIC_LIBRARY" OR
       fd2_release_sdl_type STREQUAL "SHARED_LIBRARY")
        file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/release-target-$<CONFIG>.json" CONTENT
"{\n  \"schema\": \"fd2-release-cmake-target-v1\",\n  \"configuration\": \"$<CONFIG>\",\n  \"architecture\": \"${CMAKE_GENERATOR_PLATFORM}\",\n  \"pointer_size\": ${CMAKE_SIZEOF_VOID_P},\n  \"executable\": \"$<TARGET_FILE:fd2sdl>\",\n  \"source_root\": \"${PROJECT_SOURCE_DIR}\",\n  \"sdl_target\": \"${fd2_release_sdl_target}\",\n  \"sdl_type\": \"${fd2_release_sdl_type}\",\n  \"sdl_target_file\": \"$<TARGET_FILE:${fd2_release_sdl_target}>\",\n  \"sdl_link_file\": \"$<TARGET_LINKER_FILE:${fd2_release_sdl_target}>\",\n  \"sdl_package\": \"${fd2_release_sdl_package}\",\n  \"compiler\": \"${CMAKE_CXX_COMPILER}\",\n  \"compiler_version\": \"${CMAKE_CXX_COMPILER_VERSION}\",\n  \"generator\": \"${CMAKE_GENERATOR}\"\n}\n")
    endif()
endif()
