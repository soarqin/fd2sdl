# 真正的主程序在无法选择音频后端时仍须完成快速入口；不作为原生窗口验收。
if(NOT DEFINED APP OR NOT DEFINED ROOT)
    message(FATAL_ERROR "APP and ROOT are required")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        --unset=FD2SDL_APP_PCM_CAPTURE_DIR
        --unset=FD2SDL_APP_PCM_CAPTURE_MAX_MS
        --unset=FD2SDL_APP_PCM_CAPTURE_QUIET
        --unset=FD2SDL_FIELD_RNG_SEED
        --unset=FD2SDL_SAVE_PATH
        SDL_VIDEODRIVER=dummy
        SDL_AUDIODRIVER=fd2-a8-intentionally-unavailable
        FD2SDL_TEST_AUDIO_QUIET=1
        "${APP}" --field-play-once
    WORKING_DIRECTORY "${ROOT}"
    RESULT_VARIABLE app_result
    OUTPUT_VARIABLE app_output
    ERROR_VARIABLE app_error
    TIMEOUT 55)
if(NOT "${app_result}" STREQUAL "0")
    message(FATAL_ERROR "audio unavailable quick entry failed (${app_result}):\n${app_output}${app_error}")
endif()
if(NOT app_output MATCHES "audio: null backend, 48000 Hz")
    message(FATAL_ERROR "null audio backend was not observed:\n${app_output}${app_error}")
endif()
message(STATUS "unavailable audio backend: formal quick entry completed with discard null audio")
