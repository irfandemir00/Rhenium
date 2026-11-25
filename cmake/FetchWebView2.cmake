include(CMakeParseArguments)

set(RHENIUM_WEBVIEW2_VERSION "1.0.4370.26" CACHE STRING "Microsoft.Web.WebView2 NuGet package version")
set(RHENIUM_WEBVIEW2_URL
    "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${RHENIUM_WEBVIEW2_VERSION}/microsoft.web.webview2.${RHENIUM_WEBVIEW2_VERSION}.nupkg"
)

set(_webview2_arch "x64")
if(CMAKE_VS_PLATFORM_NAME)
    string(TOLOWER "${CMAKE_VS_PLATFORM_NAME}" _vs_plat)
    if(_vs_plat STREQUAL "arm64")
        set(_webview2_arch "arm64")
    elseif(_vs_plat STREQUAL "win32" OR _vs_plat STREQUAL "x86")
        set(_webview2_arch "x86")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
    set(_webview2_arch "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(_webview2_arch "x86")
endif()

if(DEFINED ENV{RHENIUM_WEBVIEW2_CACHE_DIR} AND NOT "$ENV{RHENIUM_WEBVIEW2_CACHE_DIR}" STREQUAL "")
    set(_rhenium_default_wv2_cache "$ENV{RHENIUM_WEBVIEW2_CACHE_DIR}")
else()
    set(_rhenium_default_wv2_cache "${CMAKE_BINARY_DIR}/_deps")
endif()
set(RHENIUM_WEBVIEW2_CACHE_DIR "${_rhenium_default_wv2_cache}" CACHE PATH "Directory to cache downloaded WebView2 SDK")
file(TO_CMAKE_PATH "${RHENIUM_WEBVIEW2_CACHE_DIR}" _webview2_cache_dir)

set(_webview2_root "${_webview2_cache_dir}/webview2")
set(_webview2_nupkg "${_webview2_cache_dir}/microsoft.web.webview2.${RHENIUM_WEBVIEW2_VERSION}.nupkg")
set(_webview2_header "${_webview2_root}/build/native/include/WebView2.h")
set(_webview2_loader "${_webview2_root}/build/native/${_webview2_arch}/WebView2Loader.dll")

if(NOT EXISTS "${_webview2_header}" OR NOT EXISTS "${_webview2_loader}")
    file(MAKE_DIRECTORY "${_webview2_cache_dir}")
    if(NOT EXISTS "${_webview2_nupkg}")
        set(_dl_ok FALSE)
        set(_dl_code "")
        set(_dl_msg "")
        foreach(_attempt RANGE 1 4)
            message(STATUS "Rhenium: Downloading Microsoft.Web.WebView2 ${RHENIUM_WEBVIEW2_VERSION} (attempt ${_attempt}/4)...")
            file(DOWNLOAD "${RHENIUM_WEBVIEW2_URL}" "${_webview2_nupkg}"
                SHOW_PROGRESS
                STATUS _dl_status
                TLS_VERIFY ON
                INACTIVITY_TIMEOUT 30
                TIMEOUT 120
            )
            list(GET _dl_status 0 _dl_code)
            if(_dl_code EQUAL 0)
                set(_dl_ok TRUE)
                break()
            endif()
            list(GET _dl_status 1 _dl_msg)
            file(REMOVE "${_webview2_nupkg}")
            message(WARNING "WebView2 download attempt ${_attempt} failed (${_dl_code}): ${_dl_msg}")
            if(_attempt LESS 4)
                execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 2)
            endif()
        endforeach()

        if(NOT _dl_ok)
            find_program(_rhenium_curl_exe curl)
            if(_rhenium_curl_exe)
                message(STATUS "Rhenium: Falling back to curl for WebView2 download...")
                execute_process(
                    COMMAND "${_rhenium_curl_exe}" -fsSL --retry 3 --retry-delay 2
                        -o "${_webview2_nupkg}" "${RHENIUM_WEBVIEW2_URL}"
                    RESULT_VARIABLE _curl_rc
                )
                if(_curl_rc EQUAL 0 AND EXISTS "${_webview2_nupkg}")
                    set(_dl_ok TRUE)
                else()
                    file(REMOVE "${_webview2_nupkg}")
                endif()
            endif()
        endif()

        if(NOT _dl_ok)
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
