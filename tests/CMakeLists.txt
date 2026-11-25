add_executable(rhenium_core_tests
    test_core.cpp
)

target_link_libraries(rhenium_core_tests PRIVATE
    rhenium::core
)

target_compile_options(rhenium_core_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
)

add_test(NAME rhenium_core_tests COMMAND rhenium_core_tests)
