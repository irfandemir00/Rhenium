add_executable(rhenium_showcase WIN32
    src/main.cpp
)

target_link_libraries(rhenium_showcase PRIVATE
    rhenium::rhenium
)

rhenium_embed_assets(
    TARGET rhenium_showcase
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

rhenium_copy_webview2_loader(rhenium_showcase)

add_custom_command(TARGET rhenium_showcase POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/rhenium.json"
        "$<TARGET_FILE_DIR:rhenium_showcase>/rhenium.json"
    COMMENT "Copying rhenium.json for rhenium_showcase"
    VERBATIM
)

target_compile_options(rhenium_showcase PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
)

target_link_options(rhenium_showcase PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/ENTRY:mainCRTStartup>
)
