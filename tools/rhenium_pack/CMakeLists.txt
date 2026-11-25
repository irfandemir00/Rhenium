add_executable(rhenium_pack
    main.cpp
)

target_link_libraries(rhenium_pack PRIVATE
    shell32
)

target_compile_definitions(rhenium_pack PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_options(rhenium_pack PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
)
