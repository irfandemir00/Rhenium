add_executable(rhenium_cli
    main.cpp
    process.cpp
    templates.cpp
)

set_target_properties(rhenium_cli PROPERTIES
    OUTPUT_NAME "rhenium"
)

string(REPLACE "\\" "/" _rhenium_root_slash "${CMAKE_SOURCE_DIR}")

target_link_libraries(rhenium_cli PRIVATE
    nlohmann_json::nlohmann_json
    shell32
)

target_compile_definitions(rhenium_cli PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    "RHENIUM_DEFAULT_SOURCE_DIR=\"${_rhenium_root_slash}\""
    "RHENIUM_VERSION=\"${PROJECT_VERSION}\""
)

target_compile_options(rhenium_cli PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
)
