# 已确认冷定义以 JSON 为唯一输入；构建不读取 FD2.EXE 或原版资源。
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
file(GLOB FD2_STATIC_TABLES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/data/static/tables/*.json")
file(GLOB FD2_STATIC_FLOWS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/data/static/flows/*.json")
set(FD2_STATIC_INCLUDE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(FD2_STATIC_OUTPUTS "${FD2_STATIC_INCLUDE_ROOT}/static_data/sources.json")
foreach(input IN LISTS FD2_STATIC_TABLES)
    get_filename_component(stem "${input}" NAME_WE)
    list(APPEND FD2_STATIC_OUTPUTS "${FD2_STATIC_INCLUDE_ROOT}/static_data/${stem}.inc")
endforeach()
foreach(input IN LISTS FD2_STATIC_FLOWS)
    get_filename_component(stem "${input}" NAME_WE)
    list(APPEND FD2_STATIC_OUTPUTS "${FD2_STATIC_INCLUDE_ROOT}/static_data/${stem}_flows.inc")
endforeach()
add_custom_command(OUTPUT ${FD2_STATIC_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} -B "${PROJECT_SOURCE_DIR}/tools/static_data/generate.py"
        --root "${PROJECT_SOURCE_DIR}" --output "${FD2_STATIC_INCLUDE_ROOT}/static_data"
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${FD2_STATIC_OUTPUTS}
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/static_data/generate.py"
        "${PROJECT_SOURCE_DIR}/data/static/manifest.json" ${FD2_STATIC_TABLES} ${FD2_STATIC_FLOWS}
    VERBATIM)
add_custom_target(fd2_static_data DEPENDS ${FD2_STATIC_OUTPUTS})
include_directories("${FD2_STATIC_INCLUDE_ROOT}")
