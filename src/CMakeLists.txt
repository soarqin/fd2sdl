cmake_minimum_required(VERSION 3.16)
project(fd2sdl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 优先使用系统安装的 SDL3；找不到则给出明确提示
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
    # 退而求其次：pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL3 IMPORTED_TARGET sdl3)
    endif()
endif()

add_executable(fd2sdl
    main.c
    archive.c
    image.c
    animation.c
    vga.c
    palette.c
)

if(SDL3_FOUND)
    target_link_libraries(fd2sdl PRIVATE SDL3::SDL3)
elseif(TARGET PkgConfig::SDL3)
    target_link_libraries(fd2sdl PRIVATE PkgConfig::SDL3)
else()
    message(FATAL_ERROR
        "SDL3 not found. Install SDL3 (e.g. build from source) or set SDL3_DIR.")
endif()

# 让可执行文件能在源码树根目录运行并找到 original_game/
set_target_properties(fd2sdl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
