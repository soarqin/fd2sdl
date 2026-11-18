add_executable(fd2_bgm_test
    tests/bgm_test.c
    src/bgm.cpp
    src/audio.c
    src/audio_pcm_capture.c
    src/archive.c
    src/third_party/nukedopl3.c
    src/third_party/model_ail.c
)
target_link_libraries(fd2_bgm_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME bgm COMMAND fd2_bgm_test)
set_tests_properties(bgm PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_executable(fd2_field_game_attack_test
    tests/field_game_attack_test.c
    $<TARGET_OBJECTS:fd2sdl_core>
)
target_link_libraries(fd2_field_game_attack_test PRIVATE
    ${FD2SDL_SDL3_TARGET}
)
add_test(NAME field_game_attack COMMAND fd2_field_game_attack_test)
set_tests_properties(field_game_attack PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(fd2_field_magic_commit_test
    tests/field_magic_commit_test.c
    $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_field_magic_commit_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_magic_commit COMMAND fd2_field_magic_commit_test)
set_tests_properties(field_magic_commit PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_executable(fd2_field_turn_test
    tests/field_turn_test.c
    $<TARGET_OBJECTS:fd2sdl_core>
)
target_link_libraries(fd2_field_turn_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_turn COMMAND fd2_field_turn_test)
set_tests_properties(field_turn PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_executable(fd2_field_save_test
    tests/field_save_test.c
    $<TARGET_OBJECTS:fd2sdl_core>
)
target_link_libraries(fd2_field_save_test PRIVATE
    ${FD2SDL_SDL3_TARGET}
)
add_test(NAME field_save COMMAND fd2_field_save_test)
set_tests_properties(field_save PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_executable(fd2_battle_animation_test
    tests/battle_animation_test.c
    src/figani.c src/magic_fx.c src/tile.c src/field_page.c src/field_info.c src/field_visual.c src/field_item.c src/field_magic.c src/field_unit.c src/field_unit_base.c
    src/field_unit_stats.c src/field_attack.c src/field_path.c src/field_move_profile.c
    src/map_sprite.c src/image.c src/archive.c src/portrait.c src/text.c src/font.c src/field.c src/shape.c
    src/audio_pcm.c src/audio.c src/audio_pcm_capture.c src/field_effect.c src/field_audio.c)
target_link_libraries(fd2_battle_animation_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME battle_animation COMMAND fd2_battle_animation_test)

add_executable(fd2_field_prepare_play_test
    tests/field_prepare_play_test.c
    $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_field_prepare_play_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_prepare_play COMMAND fd2_field_prepare_play_test)
set_tests_properties(field_prepare_play PROPERTIES
    TIMEOUT 30 WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_executable(fd2_scene_handoff_test
    tests/scene_handoff_test.c
    $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_scene_handoff_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME scene_handoff COMMAND fd2_scene_handoff_test)
add_test(NAME scene_transition COMMAND fd2_scene_handoff_test --b5)
set_tests_properties(scene_transition PROPERTIES
    TIMEOUT 30 WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
set_tests_properties(scene_handoff PROPERTIES
    TIMEOUT 30 WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_executable(fd2_scene_cinematic_test tests/scene_cinematic_test.c $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_scene_cinematic_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME scene_cinematic COMMAND fd2_scene_cinematic_test)
set_tests_properties(scene_cinematic PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

# 正式快速入口：无效音频后端不得使玩法初始化失败；dummy video不算窗口验收。
add_test(NAME audio_unavailable_field_quick_entry
    COMMAND ${CMAKE_COMMAND} -DAPP=$<TARGET_FILE:fd2sdl>
        -DROOT=${PROJECT_SOURCE_DIR}
        -P ${PROJECT_SOURCE_DIR}/tests/audio_unavailable_field_once.cmake)
set_tests_properties(audio_unavailable_field_quick_entry PROPERTIES
    TIMEOUT 60 LABELS "audio;integration;headless")

add_test(NAME title_animation_owner COMMAND fd2_main_title_resource_test --animation-owner)
add_test(NAME resource COMMAND fd2_resource_test)
add_test(NAME save COMMAND fd2_save_test)
set_tests_properties(resource save battle_animation PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 30)

# 仅该测试重新编译正式循环并替换依赖，兼容 MSVC，不给生产代码增加钩子。
add_executable(fd2_field_play_result_test
    tests/field_play_result_test.c
    src/field_play.c
    $<TARGET_OBJECTS:fd2sdl_core>
)
target_link_libraries(fd2_field_play_result_test PRIVATE ${FD2SDL_SDL3_TARGET})
target_compile_definitions(fd2_field_play_result_test PRIVATE
    fd2_delay_ms=__wrap_fd2_delay_ms
    fd2_wait_ticks=__wrap_fd2_wait_ticks
    fd2_vga_delay_ms=__wrap_fd2_vga_delay_ms
    fd2_vga_wait_ticks=__wrap_fd2_vga_wait_ticks
    fd2_bios_tick_low_ns=__wrap_fd2_bios_tick_low_ns
    fd2_bios_tick_signed_ns=__wrap_fd2_bios_tick_signed_ns
    fd2_field_water_tick_live=__wrap_fd2_field_water_tick_live
    fd2_input_clear_keys=__wrap_fd2_input_clear_keys
    fd2_input_take_key=__wrap_fd2_input_take_key
    fd2_input_has_any_key=__wrap_fd2_input_has_any_key
    fd2_field_game_render=__wrap_fd2_field_game_render
    fd2_field_game_runtime_refresh=__wrap_fd2_field_game_runtime_refresh
    fd2_field_game_runtime_menu_refresh=__wrap_fd2_field_game_runtime_menu_refresh
    fd2_field_round_play=__wrap_fd2_field_round_play
    fd2_field_phase_play=__wrap_fd2_field_phase_play
    fd2_field_round_is_ready=__wrap_fd2_field_round_is_ready
    fd2_field_phase_is_ready=__wrap_fd2_field_phase_is_ready
    fd2_field_game_close=__wrap_fd2_field_game_close
    fd2_field_game_animate_command=__wrap_fd2_field_game_animate_command
    fd2_field_game_open=__wrap_fd2_field_game_open
    fd2_field_game_open_from_roster=__wrap_fd2_field_game_open_from_roster
    fd2_field_game_process_automatic_action_visual=__wrap_fd2_field_game_process_automatic_action_visual
    fd2_scene_play_common_step=__wrap_fd2_scene_play_common_step
    fd2_field_game_set_loot_preflight=__wrap_fd2_field_game_set_loot_preflight
    fd2_field_game_settlement_prepare=__wrap_fd2_field_game_settlement_prepare
    fd2_field_game_settlement_commit=__wrap_fd2_field_game_settlement_commit
    fd2_scene_play_stage_victory=__wrap_fd2_scene_play_stage_victory
    fd2_scene_play_game_over=__wrap_fd2_scene_play_game_over
    fd2_vga_present=__wrap_fd2_vga_present
    fd2_vga_upload_current_frame=__wrap_fd2_vga_upload_current_frame
    fd2_vga_present_timed=__wrap_fd2_vga_present_timed
    fd2_input_take_action=__wrap_fd2_input_take_action
    fd2_field_game_open_detail=__wrap_fd2_field_game_open_detail
    fd2_field_game_close_detail=__wrap_fd2_field_game_close_detail
    fd2_field_game_animate_detail=__wrap_fd2_field_game_animate_detail
    fd2_field_game_confirm_cursor=__wrap_fd2_field_game_confirm_cursor
    fd2_field_game_execute_move=__wrap_fd2_field_game_execute_move
    fd2_field_game_animate_system_menu=__wrap_fd2_field_game_animate_system_menu
    fd2_field_game_execute_storage_action=__wrap_fd2_field_game_execute_storage_action
    fd2_field_game_runtime_summary_refresh=__wrap_fd2_field_game_runtime_summary_refresh
    fd2_field_game_march=__wrap_fd2_field_game_march
    fd2_bgm_play=__wrap_fd2_bgm_play
    fd2_bgm_stop=__wrap_fd2_bgm_stop
    fd2_bgm_current_track=__wrap_fd2_bgm_current_track
    fd2_bgm_set_music_enabled=__wrap_fd2_bgm_set_music_enabled)
add_test(NAME field_play_result COMMAND fd2_field_play_result_test)
set_tests_properties(field_play_result PROPERTIES TIMEOUT 10
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(fd2_bgm_native_sequence_test tests/bgm_native_sequence_test.cpp
    src/audio.c src/audio_pcm_capture.c src/archive.c src/third_party/nukedopl3.c src/third_party/model_ail.c)
target_link_libraries(fd2_bgm_native_sequence_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME bgm_native_sequence COMMAND fd2_bgm_native_sequence_test)

add_executable(fd2_bgm_bank_resource_failure_test tests/bgm_bank_resource_failure_test.cpp
    src/audio.c src/audio_pcm_capture.c src/archive.c src/third_party/nukedopl3.c src/third_party/model_ail.c)
target_link_libraries(fd2_bgm_bank_resource_failure_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME bgm_bank_resource_failure COMMAND fd2_bgm_bank_resource_failure_test)

# 用显式缺失状态区分没有原资源与断言失败，不让 CTest 报成通过。
set(fd2_original_files)
foreach(file IN ITEMS ANI.DAT BG.DAT DATO.DAT FDFIELD.DAT FDICON.B24 FDMUS.DAT
    FDOTHER.DAT FDSHAP.DAT FDTXT.DAT FIGANI.DAT SAMPLE.AD START.SAV TAI.DAT)
    list(APPEND fd2_original_files "${PROJECT_SOURCE_DIR}/original_game/${file}")
endforeach()
set_tests_properties(bgm bgm_native_sequence bgm_bank_resource_failure field_game_attack field_magic_commit field_turn
    field_save battle_animation field_prepare_play scene_handoff scene_transition scene_cinematic
    audio_unavailable_field_quick_entry resource save field_play_result title_animation_owner PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS original-resources REQUIRED_FILES "${fd2_original_files}")
