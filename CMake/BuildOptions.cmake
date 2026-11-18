if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND MSVC_VERSION LESS 1928)
    message(FATAL_ERROR "fd2sdl requires MSVC 19.28 (v142) or newer for C11.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(FD2SDL_STATIC_RUNTIME AND NOT MSVC)
    if(MINGW)
        add_link_options(-static)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_link_options(-static-libgcc "$<$<LINK_LANGUAGE:CXX>:-static-libstdc++>")
    else()
        message(FATAL_ERROR "FD2SDL_STATIC_RUNTIME is unavailable with this toolchain.")
    endif()
endif()

if(FD2SDL_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT fd2_ipo_supported OUTPUT fd2_ipo_error LANGUAGES C CXX)
    if(NOT fd2_ipo_supported)
        message(FATAL_ERROR "FD2SDL_LTO is unavailable: ${fd2_ipo_error}")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Windows 上的 Ninja 构建不要求 PATH 另有 GNU make 来调度 LTRANS。
        add_link_options(-flto=1 -flto-partition=one)
    endif()
endif()

# magic_fx 使用 sin/cos；引用 core 的 TARGET_OBJECTS 不传递链接依赖。
# 在项目目标创建前显式链接 libm，覆盖主程序及同目录的状态机测试。
if(UNIX)
    link_libraries(m)
endif()

if(MSVC)
    add_compile_options(/utf-8)
    # 使用有显式长度或返回值检查的标准 C 库接口，保留跨平台调用。
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS=)
endif()
