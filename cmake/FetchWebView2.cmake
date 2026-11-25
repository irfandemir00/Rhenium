include(CMakeParseArguments)

set(RHENIUM_WEBVIEW2_VERSION "1.0.4370.26" CACHE STRING "Microsoft.Web.WebView2 NuGet package version")
set(RHENIUM_WEBVIEW2_URL
    "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${RHENIUM_WEBVIEW2_VERSION}/microsoft.web.webview2.${RHENIUM_WEBVIEW2_VERSION}.nupkg"
)

set(_webview2_root "${CMAKE_BINARY_DIR}/_deps/webview2")
set(_webview2_nupkg "${CMAKE_BINARY_DIR}/_deps/microsoft.web.webview2.${RHENIUM_WEBVIEW2_VERSION}.nupkg")
set(_webview2_header "${_webview2_root}/build/native/include/WebView2.h")
set(_webview2_loader "${_webview2_root}/build/native/x64/WebView2Loader.dll")

if(NOT EXISTS "${_webview2_header}" OR NOT EXISTS "${_webview2_loader}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
    if(NOT EXISTS "${_webview2_nupkg}")
        message(STATUS "Rhenium: Downloading Microsoft.Web.WebView2 ${RHENIUM_WEBVIEW2_VERSION}...")
        file(DOWNLOAD "${RHENIUM_WEBVIEW2_URL}" "${_webview2_nupkg}"
            SHOW_PROGRESS
            STATUS _dl_status
            TLS_VERIFY ON
        )
        list(GET _dl_status 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            list(GET _dl_status 1 _dl_msg)
            file(REMOVE "${_webview2_nupkg}")
            message(FATAL_ERROR "Failed to download Microsoft.Web.WebView2 (${_dl_code}): ${_dl_msg}")
        endif()
    endif()

    message(STATUS "Rhenium: Extracting Microsoft.Web.WebView2 SDK...")
    file(MAKE_DIRECTORY "${_webview2_root}")
    file(ARCHIVE_EXTRACT
        INPUT "${_webview2_nupkg}"
        DESTINATION "${_webview2_root}"
    )
endif()

if(NOT EXISTS "${_webview2_header}")
    message(FATAL_ERROR "WebView2.h not found after extraction at ${_webview2_header}")
endif()
if(NOT EXISTS "${_webview2_loader}")
    message(FATAL_ERROR "WebView2Loader.dll not found after extraction at ${_webview2_loader}")
endif()

set(RHENIUM_WEBVIEW2_INCLUDE_DIR "${_webview2_root}/build/native/include" CACHE PATH "WebView2 include directory" FORCE)
set(RHENIUM_WEBVIEW2_LOADER_DLL "${_webview2_loader}" CACHE FILEPATH "Path to x64 WebView2Loader.dll" FORCE)

if(NOT TARGET rhenium_webview2_sdk)
    add_library(rhenium_webview2_sdk INTERFACE)
    target_include_directories(rhenium_webview2_sdk SYSTEM INTERFACE "${RHENIUM_WEBVIEW2_INCLUDE_DIR}")
endif()

function(rhenium_copy_webview2_loader target_name)
    add_custom_command(TARGET "${target_name}" POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RHENIUM_WEBVIEW2_LOADER_DLL}"
            "$<TARGET_FILE_DIR:${target_name}>/WebView2Loader.dll"
        COMMENT "Copying WebView2Loader.dll for ${target_name}"
        VERBATIM
    )
endfunction()
