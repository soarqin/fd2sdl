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

add_executable(fd2sdl
    main.c
    archive.c
    image.c
    animation.c
    vga.c
    palette.c
)

target_link_libraries(fd2sdl PRIVATE ${FD2SDL_SDL3_TARGET})

# 让可执行文件能在源码树根目录运行并找到 original_game/
set_target_properties(fd2sdl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
