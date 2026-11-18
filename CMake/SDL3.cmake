option(FD2SDL_USE_SYSTEM_SDL3 "优先使用系统安装的 SDL3" ON)
set(FD2SDL_SDL3_VERSION "3.4.20" CACHE STRING "CPM 回退下载的 SDL3 版本")
set(FD2SDL_SDL3_GIT_TAG "release-${FD2SDL_SDL3_VERSION}" CACHE STRING "CPM 回退下载的 SDL3 Git tag")
set(FD2SDL_CPM_VERSION "0.43.2" CACHE STRING "自动下载的 CPM.cmake 版本")

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

    # Clang 23 新增的诊断由固定版本 SDL 的事件和 Vulkan 源码触发。
    if(MINGW AND CMAKE_C_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 23 AND
       FD2SDL_SDL3_GIT_TAG STREQUAL "release-3.4.20" AND TARGET SDL3-static)
        get_target_property(fd2_sdl_source SDL3-static SOURCE_DIR)
        set_property(SOURCE
            "${fd2_sdl_source}/src/events/SDL_events.c"
            "${fd2_sdl_source}/src/render/vulkan/SDL_render_vulkan.c"
            TARGET_DIRECTORY SDL3-static APPEND PROPERTY COMPILE_OPTIONS -Wno-unused-but-set-global)
    endif()

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
