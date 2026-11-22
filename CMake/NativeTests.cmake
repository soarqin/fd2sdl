if(WIN32)
    # 仅Win32宏桩，不创建窗口或依赖Python/WGC/SDL。
    add_executable(fd2_field_play_input_driver_focus_test
        tests/field_play_input_driver_focus_test.c)
    target_link_libraries(fd2_field_play_input_driver_focus_test PRIVATE user32 shell32)
    if(MSVC)
        target_compile_options(fd2_field_play_input_driver_focus_test PRIVATE /W4)
    else()
        target_compile_options(fd2_field_play_input_driver_focus_test PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME field_play_input_driver_focus COMMAND fd2_field_play_input_driver_focus_test)
    set_tests_properties(field_play_input_driver_focus PROPERTIES TIMEOUT 30)
    # WGC 和进程音频工具依赖 MSVC 的 Windows SDK、C++/WinRT 和 WRL。
    if(MSVC)
        add_executable(fd2_field_play_input_driver
            tests/field_play_input_driver.c tests/window_capture.cpp)
        # C++/WinRT 只用于测试工具，音乐生产后端保持 C++14。
        set_target_properties(fd2_field_play_input_driver PROPERTIES CXX_STANDARD 17)
        target_link_libraries(fd2_field_play_input_driver PRIVATE
            user32 d3d11 dxgi windowsapp dwmapi windowscodecs ole32 shell32)
        add_executable(fd2_process_audio_capture tests/process_audio_capture_win.cpp)
        target_link_libraries(fd2_process_audio_capture PRIVATE mmdevapi ole32 uuid)
        target_link_options(fd2_process_audio_capture PRIVATE
            /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup)
        add_executable(fd2_process_audio_capture_test tests/process_audio_capture_test.cpp)
        add_executable(fd2_window_capture_series_test tests/window_capture_series_test.cpp)
        foreach(fd2_test IN ITEMS fd2_process_audio_capture fd2_process_audio_capture_test fd2_window_capture_series_test)
            set_target_properties(${fd2_test} PROPERTIES CXX_STANDARD 17)
            target_compile_definitions(${fd2_test} PRIVATE
                _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A00000A)
            target_compile_options(${fd2_test} PRIVATE /W4)
        endforeach()
        target_link_libraries(fd2_process_audio_capture_test PRIVATE mmdevapi ole32 uuid)
        target_link_libraries(fd2_window_capture_series_test PRIVATE
            user32 d3d11 dxgi windowsapp dwmapi windowscodecs ole32 shell32)
        add_test(NAME process_audio_capture COMMAND fd2_process_audio_capture_test)
        add_test(NAME window_capture_series COMMAND fd2_window_capture_series_test)
        set_tests_properties(process_audio_capture window_capture_series PROPERTIES
            TIMEOUT 30 LABELS "process;evidence")
        add_test(NAME graphics_job
            COMMAND ${Python3_EXECUTABLE} -B
                ${PROJECT_SOURCE_DIR}/tests/field_play_job_test.py
                --driver $<TARGET_FILE:fd2_field_play_input_driver>
                --artifacts ${CMAKE_BINARY_DIR}/test-artifacts/graphics-job)
        set_tests_properties(graphics_job PROPERTIES
            TIMEOUT 60 LABELS "integration;process" RUN_SERIAL TRUE)
        if(FD2SDL_TEST_ORIGINAL)
            execute_process(COMMAND ${Python3_EXECUTABLE} -c "import PIL"
                RESULT_VARIABLE fd2_pillow_status ERROR_QUIET)
            if(fd2_pillow_status EQUAL 0)
                add_test(NAME launcher_window COMMAND fd2_field_play_input_driver run-job 120000
                    ${Python3_EXECUTABLE} -B "${PROJECT_SOURCE_DIR}/tests/launcher_windows_test.py"
                    $<TARGET_FILE:fd2sdl> $<TARGET_FILE:fd2_field_play_input_driver>
                    "${CMAKE_BINARY_DIR}/test-artifacts/launcher")
                set_tests_properties(launcher_window PROPERTIES RUN_SERIAL TRUE TIMEOUT 140
                    LABELS "e2e;graphics;original-resources" REQUIRED_FILES "${fd2_original_files}")
            elseif(FD2SDL_REQUIRE_GUI_TESTS)
                message(FATAL_ERROR "The Windows launcher WGC test requires Python Pillow.")
            else()
                message(STATUS "Windows launcher WGC test unavailable: missing Pillow")
            endif()
            add_test(NAME field_play_input
                COMMAND ${Python3_EXECUTABLE} -B
                    ${PROJECT_SOURCE_DIR}/tests/field_play_input_test.py
                    --driver $<TARGET_FILE:fd2_field_play_input_driver>
                    --game $<TARGET_FILE:fd2sdl>
                    --root ${PROJECT_SOURCE_DIR}
                    --artifacts ${CMAKE_BINARY_DIR}/test-artifacts/field-play-input)
            set_tests_properties(field_play_input PROPERTIES
                TIMEOUT 180 LABELS "e2e;graphics;original-resources" RUN_SERIAL TRUE
                REQUIRED_FILES "${fd2_original_files}")
        endif()
    elseif(FD2SDL_REQUIRE_GUI_TESTS)
        message(FATAL_ERROR "Windows GUI tests require MSVC with the Windows SDK and C++/WinRT.")
    else()
        message(STATUS "Windows WGC and process-audio tests require an MSVC build.")
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # 依赖齐备时另注册 Xvfb/XTest 回归，发送真实按键而不调用 field_game API。
    find_package(X11 QUIET)
    find_program(FD2SDL_XVFB_EXECUTABLE Xvfb)
    find_program(FD2SDL_XVFB_RUN_EXECUTABLE xvfb-run)
    find_program(FD2SDL_XAUTH_EXECUTABLE xauth)
    if(X11_FOUND AND X11_Xtst_FOUND AND FD2SDL_XVFB_EXECUTABLE AND
       FD2SDL_XVFB_RUN_EXECUTABLE AND FD2SDL_XAUTH_EXECUTABLE)
        set(fd2_xvfb "${FD2SDL_XVFB_RUN_EXECUTABLE}" -a -s "-screen 0 1024x768x24")
        add_executable(fd2_native_window_test tests/native_window_test.c src/input.c)
        target_link_libraries(fd2_native_window_test PRIVATE X11::Xtst ${FD2SDL_SDL3_TARGET})
        add_test(NAME native_window COMMAND ${fd2_xvfb} ${CMAKE_COMMAND} -E env
            SDL_VIDEODRIVER=x11 $<TARGET_FILE:fd2_native_window_test>)
        set_tests_properties(native_window PROPERTIES TIMEOUT 20 LABELS graphics RUN_SERIAL TRUE)
        if(FD2SDL_TEST_ORIGINAL)
            add_executable(fd2_field_play_input_driver tests/field_play_input_driver.c)
            target_link_libraries(fd2_field_play_input_driver PRIVATE X11::Xtst)
            add_test(NAME field_play_input COMMAND ${fd2_xvfb} sh
                "${PROJECT_SOURCE_DIR}/tests/field_play_input_test.sh"
                $<TARGET_FILE:fd2_field_play_input_driver> $<TARGET_FILE:fd2sdl>
                "${PROJECT_SOURCE_DIR}")
            set_tests_properties(field_play_input PROPERTIES TIMEOUT 90
                LABELS "e2e;graphics;original-resources" RUN_SERIAL TRUE
                REQUIRED_FILES "${fd2_original_files}")
        endif()
    elseif(FD2SDL_REQUIRE_GUI_TESTS)
        message(FATAL_ERROR "Linux GUI tests require X11, XTest, xvfb-run and xauth.")
    else()
        message(STATUS "Linux GUI tests not registered: install X11, XTest, xvfb-run and xauth.")
    endif()
endif()
