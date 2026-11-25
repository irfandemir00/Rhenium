include(CMakeParseArguments)

function(rhenium_embed_assets)
    cmake_parse_arguments(ARG "" "TARGET;DIRECTORY;EMBED" "" ${ARGN})

    if(NOT ARG_TARGET OR NOT ARG_DIRECTORY)
        message(FATAL_ERROR "rhenium_embed_assets requires TARGET and DIRECTORY arguments")
    endif()

    get_filename_component(_abs_ui_dir "${ARG_DIRECTORY}" ABSOLUTE)
    string(REPLACE "\\" "/" _abs_ui_dir_slash "${_abs_ui_dir}")

    set(_should_embed "${RHENIUM_EMBED_ASSETS}")
    if(DEFINED ARG_EMBED)
        set(_should_embed "${ARG_EMBED}")
    endif()

    if(_should_embed)
        file(GLOB_RECURSE _ui_files CONFIGURE_DEPENDS "${_abs_ui_dir}/*")
        set(_gen_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/${ARG_TARGET}_embedded_assets.cpp")
        get_filename_component(_gen_dir "${_gen_cpp}" DIRECTORY)

        add_custom_command(
            OUTPUT "${_gen_cpp}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_gen_dir}"
            COMMAND "$<TARGET_FILE:rhenium_pack>" --input "${_abs_ui_dir}" --output "${_gen_cpp}"
            DEPENDS rhenium_pack ${_ui_files}
            COMMENT "Packing UI assets from ${_abs_ui_dir_slash} for ${ARG_TARGET}"
            VERBATIM
        )

        target_sources("${ARG_TARGET}" PRIVATE "${_gen_cpp}")
        target_compile_definitions("${ARG_TARGET}" PRIVATE RHENIUM_EMBEDDED_ASSETS=1)
    else()
        set(_dev_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/${ARG_TARGET}_dev_assets.cpp")
        file(CONFIGURE
            OUTPUT "${_dev_cpp}"
            CONTENT "#include \"rhenium/core/assets.hpp\"\nnamespace {\nstruct RheniumDevUiRegistrar {\n    RheniumDevUiRegistrar() {\n        rhenium::set_default_dev_ui_dir(\"${_abs_ui_dir_slash}\");\n    }\n} g_rhenium_dev_ui_registrar;\n}\n"
            @ONLY
        )
        target_sources("${ARG_TARGET}" PRIVATE "${_dev_cpp}")
        target_compile_definitions("${ARG_TARGET}" PRIVATE
            RHENIUM_EMBEDDED_ASSETS=0
            "RHENIUM_DEV_UI_DIR=\"${_abs_ui_dir_slash}\""
        )
    endif()
endfunction()
