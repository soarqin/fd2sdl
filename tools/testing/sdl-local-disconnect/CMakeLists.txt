cmake_minimum_required(VERSION 3.24)
project(fd2_sdl_local_disconnect C)

# 仅验证本机已封存的SDL内部ABI，不属于普通App或通用SDL兼容接口。
if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "This pinned-static SDL internal probe requires Windows/MSVC")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Use the x64 ABI of the pinned SDL3-static.lib")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES Debug CACHE STRING "Pinned Debug static ABI" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Configure this probe with CMAKE_BUILD_TYPE=Debug")
endif()

# 三个路径必须由调用者提供；不回退到另一份SDK、源码或生成配置头。
set(SDL3_SOURCE_DIR "" CACHE PATH "Source tree matching the pinned SDL static library")
set(SDL3_BUILD_CONFIG_DIR "" CACHE PATH "Debug directory containing SDL_build_config.h")
if(NOT SDL3_DIR OR NOT EXISTS "${SDL3_DIR}/SDL3Config.cmake")
    message(FATAL_ERROR "Provide SDL3_DIR for the pinned SDL CMake package")
endif()
if(NOT EXISTS "${SDL3_SOURCE_DIR}/src/audio/SDL_sysaudio.h" OR
   NOT EXISTS "${SDL3_SOURCE_DIR}/src/audio/SDL_audio.c")
    message(FATAL_ERROR "Provide the matching real SDL3_SOURCE_DIR")
endif()
if(NOT EXISTS "${SDL3_BUILD_CONFIG_DIR}/SDL_build_config.h")
    message(FATAL_ERROR "Provide the matching Debug SDL3_BUILD_CONFIG_DIR")
endif()
# 私有struct/原子与编译配置必须和已验证静态库配对；仅库SHA不保护头文件ABI。
function(fd2_require_sdk_file path expected_sha)
    file(SHA256 "${path}" actual_sha)
    if(NOT actual_sha STREQUAL expected_sha)
        message(FATAL_ERROR "SDL private ABI input differs from the validated SDK: ${path}")
    endif()
endfunction()
fd2_require_sdk_file("${SDL3_SOURCE_DIR}/src/audio/SDL_sysaudio.h"
    "9646c302784455e879ac12171e37272c0cf4eb1ec54c6390335b2b5f397652e6")
fd2_require_sdk_file("${SDL3_SOURCE_DIR}/src/SDL_internal.h"
    "bf9d044d8513faf54c38a5bb02792c3b0de8ee0e0d477ac7f0a8b599a93c3c63")
fd2_require_sdk_file("${SDL3_BUILD_CONFIG_DIR}/SDL_build_config.h"
    "5b75342206bf4ddb931aa66923f6c5d795ec2c72227595b4a430dc7b1fe3e36b")
find_package(SDL3 CONFIG REQUIRED)
if(NOT TARGET SDL3::SDL3-static)
    message(FATAL_ERROR "The pinned SDL3::SDL3-static target is required")
endif()
get_target_property(fd2_sdl_static SDL3::SDL3-static IMPORTED_LOCATION_DEBUG)
if(NOT fd2_sdl_static OR NOT EXISTS "${fd2_sdl_static}")
    message(FATAL_ERROR "The pinned Debug static library is unavailable")
endif()
file(SHA256 "${fd2_sdl_static}" fd2_sdl_static_sha)
if(fd2_sdl_static_sha STREQUAL "5d920411374519962ce4dacbd04c86d210a4c71dea243e5d922779392dcbb090")
    # 历史库及原三header合同不变；不以本次Git查验回签其source HEAD。
    set(fd2_sdl_sdk_identity "historical-5d920411")
elseif(fd2_sdl_static_sha STREQUAL "89b0bb041ca201c97e1e79b960c3bc642762356d0255e2886d93cb11bc5c2839")
    # 当前独立资格：SDL3.2.12/5ac37a8；2813f263仅CPM缓存目录名。
    # F5源/COFF-C13绑定、9pure及2026-11-17 owned Zombie路径分别留证。
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SDL3_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE fd2_sdl_git_exit OUTPUT_VARIABLE fd2_sdl_git_head
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SDL3_SOURCE_DIR}" status --porcelain
        RESULT_VARIABLE fd2_sdl_status_exit OUTPUT_VARIABLE fd2_sdl_status
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT fd2_sdl_git_exit EQUAL 0 OR NOT fd2_sdl_status_exit EQUAL 0 OR
       NOT fd2_sdl_git_head STREQUAL "5ac37a8ffcf89da390404c1016833d56e2d67ae4" OR fd2_sdl_status)
        message(FATAL_ERROR "Current 89b SDK requires clean SDL release-3.2.12 source at 5ac37a8")
    endif()
    fd2_require_sdk_file("${SDL3_SOURCE_DIR}/src/audio/SDL_audio.c"
        "028cc7a595df83918964607136e16f030195449bfae97631405693df77dbc9a6")
    fd2_require_sdk_file("${SDL3_SOURCE_DIR}/src/audio/SDL_audiocvt.c"
        "64b33626d25120cd967a0e0a0ea8d662aa2ca2085414ebd43fa083d5a4839bf9")
    fd2_require_sdk_file("${SDL3_SOURCE_DIR}/include/SDL3/SDL_atomic.h"
        "faa1f4fd73f273fe96dd9718142096dcb2d22ca1c7820899a045e1636bf75a44")
    fd2_require_sdk_file("${SDL3_SOURCE_DIR}/include/SDL3/SDL_audio.h"
        "c1a07cfe3b2012b471c873c0126314f1efd14737b6e0fc25a9b26689eb90cfa3")
    set(fd2_sdl_sdk_identity "SDL-3.2.12-5ac37a8-89b0bb04")
else()
    message(FATAL_ERROR "SDL3-static.lib differs from both explicitly validated local SDK identities")
endif()
message(STATUS "Private SDL SDK identity: ${fd2_sdl_sdk_identity}")

get_filename_component(fd2_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(fd2_src "${fd2_repo_root}/src")
add_executable(fd2_audio_device_loss_probe
    "${fd2_src}/tests/audio_device_loss_probe.c"
    "${fd2_src}/tests/sdl_local_disconnect.c"
    "${fd2_src}/audio_pcm_capture.c"
    "${fd2_src}/bios_clock.c" "${fd2_src}/ail_timer.c"
    "${fd2_src}/audio_dig.c" "${fd2_src}/sb_dma_provider.c"
    "${fd2_src}/sb_channel.c" "${fd2_src}/audio_sb.c" "${fd2_src}/mixdown_dc.c")
add_executable(fd2_sdl_local_disconnect_test
    "${fd2_src}/tests/sdl_local_disconnect_test.c")
target_compile_definitions(fd2_sdl_local_disconnect_test PRIVATE SDL_MAIN_HANDLED=1)
foreach(fd2_target IN ITEMS fd2_audio_device_loss_probe fd2_sdl_local_disconnect_test)
    target_include_directories(${fd2_target} PRIVATE
        "${fd2_src}" "${SDL3_SOURCE_DIR}/src" "${SDL3_BUILD_CONFIG_DIR}")
    target_link_libraries(${fd2_target} PRIVATE SDL3::SDL3-static)
    target_compile_options(${fd2_target} PRIVATE /W4 /utf-8)
    set_target_properties(${fd2_target} PROPERTIES
        COMPILE_PDB_NAME "${fd2_target}"
        COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdb")
endforeach()

enable_testing()
add_test(NAME private_sdl_local_disconnect COMMAND fd2_sdl_local_disconnect_test)
set_tests_properties(private_sdl_local_disconnect PROPERTIES TIMEOUT 10)
# 真实设备probe不注册CTest；默认dry，只有显式--execute才打开设备。
