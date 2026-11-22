cmake_minimum_required(VERSION 3.21)

file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" fd2_source_root)
file(REAL_PATH "${CMAKE_CURRENT_BINARY_DIR}" fd2_build_root)
file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src" fd2_source_code)
if(fd2_build_root STREQUAL fd2_source_root OR fd2_build_root STREQUAL fd2_source_code)
    message(FATAL_ERROR "Choose a separate build directory with cmake -S . -B <build-dir>.")
endif()

project(fd2sdl VERSION 0.2.0 LANGUAGES C CXX)
configure_file(src/version.h.in generated/version.h @ONLY)

set(FD2_SRC "${PROJECT_SOURCE_DIR}/src")
option(BUILD_TESTING "Build and register fd2sdl tests" OFF)
option(FD2SDL_STATIC_RUNTIME "Link the compiler runtime statically" OFF)
option(FD2SDL_LTO "Enable link-time optimization for fd2sdl targets" OFF)

# CPM 静态 SDL 与项目目标使用同一种 MSVC CRT。
if(FD2SDL_STATIC_RUNTIME AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(CMake/SDL3.cmake)
include(CMake/BuildOptions.cmake)
include(CMake/StaticData.cmake)
include(CMake/Application.cmake)

include(CTest)
if(BUILD_TESTING)
    include(CMake/Tests.cmake)
else()
    # 重配已有目录时不让 CTest 继续读旧测试清单。
    file(REMOVE "${PROJECT_BINARY_DIR}/CTestTestfile.cmake")
endif()
