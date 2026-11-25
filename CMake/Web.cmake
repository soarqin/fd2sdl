include(FetchContent)

# 仅在 Web 构建时获取 ZIP 读取器，网页和原版数据均可在本地运行。
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(fd2_zipjs
    URL https://registry.npmjs.org/@zip.js/zip.js/-/zip.js-2.25.0.tgz
    URL_HASH SHA256=ef2e62d9c6f2fda0c2443d4b5e35beb47aa183c460737aa79eeb841bc9286a79
    TLS_VERIFY ON)
FetchContent_MakeAvailable(fd2_zipjs)

set(fd2_web_dir "${PROJECT_BINARY_DIR}/web")
if(CMAKE_CONFIGURATION_TYPES)
    string(APPEND fd2_web_dir "/$<CONFIG>")
endif()
set_target_properties(fd2sdl PROPERTIES SUFFIX ".mjs" RUNTIME_OUTPUT_DIRECTORY "${fd2_web_dir}")
# Asyncify 让现有 SDL 等待交还事件循环。主栈容纳 VGA 页和嵌套战场流程。
target_link_options(fd2sdl PRIVATE
    --no-entry -sENVIRONMENT=web
    -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=65536 -sSTACK_SIZE=2097152
    -sALLOW_MEMORY_GROWTH=1
    -lidbfs.js "-sEXPORTED_RUNTIME_METHODS=['ccall','FS','IDBFS']")

# 链接器同时生成 .mjs 和 .wasm。登记第二个产物，缺件时 Ninja 会重新链接。
add_custom_command(TARGET fd2sdl POST_BUILD
    BYPRODUCTS "${fd2_web_dir}/fd2sdl.wasm"
    COMMAND "${CMAKE_COMMAND}" -E sha256sum "${fd2_web_dir}/fd2sdl.wasm"
    VERBATIM)

function(fd2_web_copy source name)
    set(output "${fd2_web_dir}/${name}")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${source}" "${output}"
        DEPENDS "${source}" VERBATIM)
    target_sources(fd2sdl PRIVATE "${output}")
endfunction()
foreach(file IN ITEMS index.html game.mjs game-data.mjs game-save.mjs)
    fd2_web_copy("${PROJECT_SOURCE_DIR}/src/web/${file}" "${file}")
endforeach()
fd2_web_copy("${fd2_zipjs_SOURCE_DIR}/dist/zip-core.min.js" "zip-core.min.js")
fd2_web_copy("${fd2_zipjs_SOURCE_DIR}/LICENSE" "LICENSE.zip.js")
file(GLOB component_licenses RELATIVE "${PROJECT_SOURCE_DIR}" CONFIGURE_DEPENDS
    "${FD2_SRC}/third_party/LICENSE.*" "${FD2_SRC}/third_party/midiseq/LICENSE.*")
foreach(file IN ITEMS LICENSE THIRD_PARTY_NOTICES.md src/third_party/README.md ${component_licenses})
    fd2_web_copy("${PROJECT_SOURCE_DIR}/${file}" "${file}")
endforeach()
foreach(file IN ITEMS LICENSE system/lib/libc/musl/COPYRIGHT system/lib/compiler-rt/LICENSE.TXT
        system/lib/libcxx/LICENSE.TXT system/lib/libcxxabi/LICENSE.TXT system/lib/llvm-libc/LICENSE.TXT)
    if(EXISTS "${EMSCRIPTEN_ROOT_PATH}/${file}")
        fd2_web_copy("${EMSCRIPTEN_ROOT_PATH}/${file}" "emscripten/${file}")
    endif()
endforeach()

if(BUILD_TESTING)
    if(FD2SDL_REQUIRE_GUI_TESTS)
        message(FATAL_ERROR "Native GUI tests require a Windows or Linux test build.")
    endif()
    find_program(FD2SDL_NODE NAMES node nodejs REQUIRED)
    execute_process(COMMAND "${FD2SDL_NODE}" -e
        "process.exit(parseInt(process.versions.node, 10) >= 22 ? 0 : 1)"
        RESULT_VARIABLE node_status)
    if(NOT node_status STREQUAL "0")
        message(FATAL_ERROR "Web tests require Node.js 22 or newer. Check FD2SDL_NODE.")
    endif()
    foreach(test IN ITEMS web_game_data web_game_save)
        add_test(NAME ${test} COMMAND "${FD2SDL_NODE}"
            "${PROJECT_SOURCE_DIR}/tests/${test}_test.mjs" "${fd2_web_dir}")
        set_tests_properties(${test} PROPERTIES TIMEOUT 20)
    endforeach()
endif()
