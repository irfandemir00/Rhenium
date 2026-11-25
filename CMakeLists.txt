cmake_minimum_required(VERSION 3.22)
project(Rhenium VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Rhenium currently targets Windows (Win32 + WebView2) only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
    set(_rhenium_default_embed ON)
else()
    set(_rhenium_default_embed OFF)
endif()

option(RHENIUM_BUILD_EXAMPLES "Build Rhenium example applications" ON)
option(RHENIUM_BUILD_TESTS "Build Rhenium unit tests" ON)
option(RHENIUM_BUILD_CLI "Build Rhenium CLI tool" ON)
option(RHENIUM_EMBED_ASSETS "Embed UI assets into executable" ${_rhenium_default_embed})

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchWebView2)
include(RheniumEmbedAssets)

add_library(rhenium_core STATIC
    src/core/config.cpp
    src/core/security.cpp
    src/core/assets.cpp
    src/core/ipc.cpp
    src/bridge/runtime_js.cpp
)
add_library(rhenium::core ALIAS rhenium_core)

target_include_directories(rhenium_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(rhenium_core PUBLIC
    nlohmann_json::nlohmann_json
)
target_compile_definitions(rhenium_core PUBLIC
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
target_compile_options(rhenium_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
)

add_library(rhenium STATIC
    src/platform/win32/webview2_loader.cpp
    src/platform/win32/native.cpp
    src/platform/win32/file_watcher.cpp
    src/platform/win32/window.cpp
    src/bridge/builtin_commands.cpp
    src/runtime/app.cpp
)
add_library(rhenium::rhenium ALIAS rhenium)

target_include_directories(rhenium
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(rhenium
    PUBLIC
        rhenium_core
    PRIVATE
        rhenium_webview2_sdk
        ole32
        oleaut32
        shlwapi
        dwmapi
        shell32
        user32
        gdi32
        uuid
        comdlg32
)
target_compile_definitions(rhenium
    PRIVATE
        "RHENIUM_VERSION=\"${PROJECT_VERSION}\""
)
target_compile_options(rhenium PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
)

add_subdirectory(tools/rhenium_pack)

if(RHENIUM_BUILD_CLI)
    add_subdirectory(tools/rhenium_cli)
endif()

if(RHENIUM_BUILD_EXAMPLES)
    add_subdirectory(examples/showcase)
endif()

if(RHENIUM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
