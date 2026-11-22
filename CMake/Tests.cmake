option(FD2SDL_TEST_ORIGINAL "Register tests that require local original game files" OFF)
option(FD2SDL_REQUIRE_GUI_TESTS "Fail configuration if native GUI tests cannot be registered" OFF)

# 测试可独立编译正式源文件；同样需要构建期生成的冷定义。
include_directories("${FD2_SRC}")

add_executable(fd2_startup_resources_test
    tests/startup_resources_test.c src/field_play.c src/field_prepare_play.c
    $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_startup_resources_test PRIVATE fd2_launcher ${FD2SDL_SDL3_TARGET})
add_test(NAME startup_resources COMMAND fd2_startup_resources_test)
set_tests_properties(startup_resources PROPERTIES TIMEOUT 30
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_test(NAME field_play_input_harness COMMAND ${Python3_EXECUTABLE} -B
    "${PROJECT_SOURCE_DIR}/tests/field_play_input_test.py" --self-test)

add_executable(fd2_main_title_resource_test
    tests/main_title_resource_test.c
    src/field_play.c
    src/field_prepare_play.c
    $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_main_title_resource_test PRIVATE fd2_launcher ${FD2SDL_SDL3_TARGET})
add_test(NAME main_title_resource COMMAND fd2_main_title_resource_test)
add_test(NAME title_audio_return COMMAND fd2_main_title_resource_test --audio-return)
set_tests_properties(main_title_resource PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 30)

add_executable(fd2_resource_test
    tests/resource_test.c
    src/archive.c
    src/image.c
    src/shape.c
)
add_test(NAME resource_synthetic COMMAND fd2_resource_test --synthetic)

add_executable(fd2_app_flow_test
    tests/app_flow_test.c
    src/app_flow.c
    src/field.c
    src/archive.c
)
add_test(NAME app_flow COMMAND fd2_app_flow_test)

add_executable(fd2_save_test
    tests/save_test.c
    src/save.c
)
add_test(NAME save_synthetic COMMAND fd2_save_test --synthetic)
set_tests_properties(save_synthetic PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(fd2_field_manual_slot_test
    tests/field_manual_slot_test.c
    src/field_manual_slot.c
    src/save.c
)
add_test(NAME field_manual_slot COMMAND fd2_field_manual_slot_test)

add_executable(fd2_input_test
    tests/input_test.c
    src/input.c
)
target_link_libraries(fd2_input_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME input COMMAND fd2_input_test)

add_executable(fd2_font_test
    tests/font_test.c
    src/font.c
    src/archive.c
)
target_link_libraries(fd2_font_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME font COMMAND fd2_font_test)

add_executable(fd2_dialog_flow_test
    tests/dialog_flow_test.c
    src/dialog_flow.c
)
target_link_libraries(fd2_dialog_flow_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME dialog_flow COMMAND fd2_dialog_flow_test)

add_executable(fd2_field_common_event_test tests/field_common_event_test.c $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_field_common_event_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_common_event COMMAND fd2_field_common_event_test)

add_executable(fd2_field_path_test
    tests/field_path_test.c
    src/field_path.c
    src/field_move.c
    src/field_move_profile.c
)
target_link_libraries(fd2_field_path_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_path COMMAND fd2_field_path_test)

add_executable(fd2_field_player_controller_test
    tests/field_player_controller_test.c
    $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_field_player_controller_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_player_controller COMMAND fd2_field_player_controller_test)

add_executable(fd2_field_game_failure_test
    tests/field_game_failure_test.c
    $<TARGET_OBJECTS:fd2sdl_core>
)
target_link_libraries(fd2_field_game_failure_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_game_failure COMMAND fd2_field_game_failure_test)

add_executable(fd2_field_item_commit_test
    tests/field_item_commit_test.c
    $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_field_item_commit_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_item_commit COMMAND fd2_field_item_commit_test)

add_executable(fd2_field_prepare_flow_domain_test
    tests/field_prepare_flow_domain_test.c $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_field_prepare_flow_domain_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_prepare_flow_domain COMMAND fd2_field_prepare_flow_domain_test)

add_executable(fd2_field_prepare_test
    tests/field_prepare_test.c $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_field_prepare_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME field_prepare COMMAND fd2_field_prepare_test)

add_executable(fd2_text_expand_test tests/text_expand_test.c src/text.c src/archive.c)
add_test(NAME text_expand COMMAND fd2_text_expand_test)

add_executable(fd2_field_rng_test
    tests/field_rng_test.c
    src/field_rng.c
)
add_test(NAME field_rng COMMAND fd2_field_rng_test)

# 单TU替换设备调用；故障/队列压力回归不打开实际设备。
add_executable(fd2_audio_lifecycle_test
    tests/audio_lifecycle_test.c src/audio_pcm_capture.c)
target_link_libraries(fd2_audio_lifecycle_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME audio_lifecycle COMMAND fd2_audio_lifecycle_test)
add_test(NAME audio_failed_install_source COMMAND fd2_audio_lifecycle_test --abandon-source)

add_executable(fd2_audio_pcm_test
    tests/audio_pcm_test.c
    src/audio_pcm.c
    src/audio.c
    src/audio_pcm_capture.c
    src/field_audio.c
    src/archive.c
)
target_link_libraries(fd2_audio_pcm_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME audio_pcm COMMAND fd2_audio_pcm_test)

# 主线程transport私有TU；mock设备/宿主时间，真实共享BIOS/AIL，不开设备。
add_executable(fd2_audio_transport_test
    tests/audio_transport_test.c)
target_link_libraries(fd2_audio_transport_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME audio_transport COMMAND fd2_audio_transport_test)

add_executable(fd2_bios_clock_test tests/bios_clock_test.c)
target_link_libraries(fd2_bios_clock_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME bios_clock COMMAND fd2_bios_clock_test)

# 同一fd2_clock的SOURCE owner/checked回执；不另编第二timer/Audio。
add_executable(fd2_bios_wait_test tests/bios_wait_test.c   src/input.c src/field_page.c)
target_link_libraries(fd2_bios_wait_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME bios_wait COMMAND fd2_bios_wait_test)

add_executable(fd2_clock_error_scene_audio_test
    tests/clock_error_scene_audio_test.c $<TARGET_OBJECTS:fd2sdl_core>)
target_link_libraries(fd2_clock_error_scene_audio_test PRIVATE ${FD2SDL_SDL3_TARGET})
add_test(NAME clock_audio_return_bridges COMMAND fd2_clock_error_scene_audio_test)

# 应用侧静音前PCM observer；全部为合成输入/设备调用mock，不开设备或GUI。
add_executable(fd2_audio_pcm_capture_test
    tests/audio_pcm_capture_test.c src/audio.c src/audio_pcm_capture.c)
add_executable(fd2_audio_pcm_capture_hook_test
    tests/audio_pcm_capture_hook_test.c src/audio_pcm_capture.c)
add_executable(fd2_audio_pcm_quantizer_test tests/audio_pcm_quantizer_test.c)
foreach(fd2_test IN ITEMS fd2_audio_pcm_capture_test fd2_audio_pcm_capture_hook_test fd2_audio_pcm_quantizer_test)
    target_link_libraries(${fd2_test} PRIVATE ${FD2SDL_SDL3_TARGET})
endforeach()
add_test(NAME audio_pcm_capture_boundaries COMMAND fd2_audio_pcm_capture_test
    "${CMAKE_CURRENT_BINARY_DIR}/pcm-boundary-artifacts")
add_test(NAME audio_pcm_capture_main_order COMMAND fd2_audio_pcm_capture_hook_test
    "${CMAKE_CURRENT_BINARY_DIR}/pcm-hook-artifacts")
add_test(NAME audio_pcm_capture_quantizer COMMAND fd2_audio_pcm_quantizer_test)
set_tests_properties(audio_pcm_capture_boundaries audio_pcm_capture_main_order audio_pcm_capture_quantizer
    PROPERTIES TIMEOUT 15 LABELS "audio;unit")

if(FD2SDL_TEST_ORIGINAL)
    include(CMake/OriginalResourceTests.cmake)
else()
    message(STATUS "Original-resource tests disabled (FD2SDL_TEST_ORIGINAL=OFF)")
endif()
include(CMake/NativeTests.cmake)

add_test(NAME static_data_generated COMMAND ${Python3_EXECUTABLE} -B
    "${PROJECT_SOURCE_DIR}/tools/static_data/generate.py" --root "${PROJECT_SOURCE_DIR}"
    --output "${FD2_STATIC_INCLUDE_ROOT}/static_data" --check)
add_test(NAME tool_regressions COMMAND ${Python3_EXECUTABLE} -B -m unittest discover
    -s "${PROJECT_SOURCE_DIR}/tools/tests")
set_tests_properties(tool_regressions PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 120)

add_executable(fd2_launcher_controls_test tests/launcher_controls_test.c src/input.c)
target_link_libraries(fd2_launcher_controls_test PRIVATE fd2_launcher)
add_test(NAME launcher_controls COMMAND fd2_launcher_controls_test)
set_tests_properties(launcher_controls PROPERTIES ENVIRONMENT
    "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy;FD2SDL_TEST_AUDIO_QUIET=0")

add_executable(fd2_gamepad_test tests/gamepad_test.c src/input.c)
add_test(NAME gamepad COMMAND fd2_gamepad_test)

add_executable(fd2_pacing_test tests/pacing_test.c src/field_play.c
    $<TARGET_OBJECTS:fd2sdl_core>)
add_test(NAME pacing COMMAND fd2_pacing_test)

add_executable(fd2_settings_test tests/settings_test.c)
add_test(NAME settings COMMAND ${Python3_EXECUTABLE} -B
    "${PROJECT_SOURCE_DIR}/tests/settings_test.py" $<TARGET_FILE:fd2_settings_test>)

get_directory_property(fd2_tests TESTS)
set_property(TEST ${fd2_tests} APPEND PROPERTY ENVIRONMENT "FD2SDL_CONFIG=none")

# 测试中的 assert 包含实际调用，Release 不得空跑。
get_directory_property(fd2_targets BUILDSYSTEM_TARGETS)
foreach(target IN LISTS fd2_targets)
    if(target MATCHES "^fd2_.*_test$")
        add_dependencies(${target} fd2_static_data)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 "/FI${PROJECT_SOURCE_DIR}/tests/test_asserts.h")
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        endif()
    endif()
endforeach()
