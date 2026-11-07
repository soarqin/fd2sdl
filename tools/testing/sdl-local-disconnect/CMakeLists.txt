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
if(NOT fd2_sdl_static_sha STREQUAL "5d920411374519962ce4dacbd04c86d210a4c71dea243e5d922779392dcbb090")
    message(FATAL_ERROR "SDL3-static.lib differs from the locally validated internal ABI")
endif()

get_filename_component(fd2_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(fd2_src "${fd2_repo_root}/src")
add_executable(fd2_audio_device_loss_probe
    "${fd2_src}/tests/audio_device_loss_probe.c"
    "${fd2_src}/tests/sdl_local_disconnect.c"
    "${fd2_src}/audio_pcm_capture.c")
add_executable(fd2_sdl_local_disconnect_test
    "${fd2_src}/tests/sdl_local_disconnect_test.c")
target_compile_definitions(fd2_sdl_local_disconnect_test PRIVATE SDL_MAIN_HANDLED=1)
foreach(fd2_target IN ITEMS fd2_audio_device_loss_probe fd2_sdl_local_disconnect_test)
    target_include_directories(${fd2_target} PRIVATE
        "${fd2_src}" "${SDL3_SOURCE_DIR}/src" "${SDL3_BUILD_CONFIG_DIR}")
    target_link_libraries(${fd2_target} PRIVATE SDL3::SDL3-static)
    target_compile_options(${fd2_target} PRIVATE /W4 /utf-8 /experimental:c11atomics)
    set_target_properties(${fd2_target} PROPERTIES
        COMPILE_PDB_NAME "${fd2_target}"
        COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdb")
endforeach()

enable_testing()
add_test(NAME private_sdl_local_disconnect COMMAND fd2_sdl_local_disconnect_test)
set_tests_properties(private_sdl_local_disconnect PROPERTIES TIMEOUT 10)
# 真实设备probe不注册CTest；默认dry，只有显式--execute才打开设备。
