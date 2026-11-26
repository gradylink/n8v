option(N8V_BACKEND_WINUI3 "Build the WinUI 3 (Windows App SDK) native backend, on Windows." ON)

set(N8V_HAVE_WINUI3 FALSE)

if(N8V_BACKEND_WINUI3 AND WIN32)
  set(_N8V_WINUI3_DIR "${CMAKE_BINARY_DIR}/winui3")
  set(_N8V_WINUI3_GEN "${_N8V_WINUI3_DIR}/gen")

  # TODO: support pre-instaled stuff
  set(_N8V_WINUI3_PACKAGES
    "microsoft.windows.cppwinrt|2.0.250303.1|955e3051b35db1c00177ed14ab6b7b995c3b32a53fdb6c184ea53b1dba66e439"
    "microsoft.windowsappsdk.base|1.8.251216001|58f0c69ad99293e7efd36b7f8c6ead0e20940a5e91866b16901b9610b08642c9"
    "microsoft.windowsappsdk.foundation|1.8.260803002|b9232041afd605b606c6f78f442d92ead0076453f1f2a3260d2b7f8089bcab0e"
    "microsoft.windowsappsdk.interactiveexperiences|1.8.260708001|496eea92d353b5d3601b67353f06dcadd6d2d9b635575acebe6e42587dbfad76"
    "microsoft.windowsappsdk.winui|1.8.260803003|bdf72c0cbc385ba6e6ab916b17d3aa00950811c9d6372d11ac2f32f1c83f2496"
    "microsoft.windows.sdk.contracts|10.0.28000.2957|9fa4ef0e9046d17ded567cc69431924ffe0dc39eed86caea543be3bc23abb971"
    "microsoft.web.webview2|1.0.4370.26|1c0dacac84b70109b3fd42f90c53a0ba942abc3a87590d7a4e0ccd86e4a06cf4"
  )

  set(_N8V_WINUI3_OK TRUE)
  if(NOT EXISTS "${_N8V_WINUI3_GEN}/winrt/Microsoft.UI.Xaml.Controls.h")
    foreach(_spec ${_N8V_WINUI3_PACKAGES})
      string(REPLACE "|" ";" _parts "${_spec}")
      list(GET _parts 0 _id)
      list(GET _parts 1 _version)
      list(GET _parts 2 _hash)
      set(_archive "${_N8V_WINUI3_DIR}/dl/${_id}.zip")
      set(_dest "${_N8V_WINUI3_DIR}/pkg/${_id}")
      if(NOT EXISTS "${_dest}")
        file(DOWNLOAD "https://api.nuget.org/v3-flatcontainer/${_id}/${_version}/${_id}.${_version}.nupkg" "${_archive}"
          EXPECTED_HASH SHA256=${_hash} TLS_VERIFY ON STATUS _status)
        list(GET _status 0 _code)
        if(NOT _code EQUAL 0)
          file(REMOVE "${_archive}")
          message(WARNING "[n8v] Could not fetch ${_id} ${_version} (${_status}) - the WinUI 3 backend is disabled")
          set(_N8V_WINUI3_OK FALSE)
          break()
        endif()
        file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_dest}")
      endif()
    endforeach()

    if(_N8V_WINUI3_OK)
      set(_cppwinrt "pkg/microsoft.windows.cppwinrt/bin/cppwinrt.exe")
      set(_runner "")
      if(NOT CMAKE_HOST_WIN32)
        find_program(_N8V_WINE NAMES wine wine64)
        if(NOT _N8V_WINE)
          message(WARNING "[n8v] cppwinrt.exe needs wine when not building on Windows - the WinUI 3 backend is disabled")
          set(_N8V_WINUI3_OK FALSE)
        endif()
        set(_runner "${_N8V_WINE}")
      endif()
    endif()

    if(_N8V_WINUI3_OK)
      set(_pkg "pkg")
      set(_in
        -in "${_pkg}/microsoft.windowsappsdk.winui/metadata"
        -in "${_pkg}/microsoft.windowsappsdk.foundation/metadata"
        -in "${_pkg}/microsoft.windowsappsdk.interactiveexperiences/metadata/10.0.18362.0"
        -in "${_pkg}/microsoft.windows.sdk.contracts/ref/netstandard2.0"
        -in "${_pkg}/microsoft.web.webview2/lib/Microsoft.Web.WebView2.Core.winmd"
      )
      message(STATUS "[n8v] Generating the C++/WinRT projection for the Windows App SDK (once)")
      execute_process(
        COMMAND ${_runner} "${_cppwinrt}" ${_in} -out gen -optimize
        RESULT_VARIABLE _result
        OUTPUT_QUIET ERROR_VARIABLE _error
        WORKING_DIRECTORY "${_N8V_WINUI3_DIR}"
      )
      if(NOT _result EQUAL 0 OR NOT EXISTS "${_N8V_WINUI3_GEN}/winrt/Microsoft.UI.Xaml.Controls.h")
        message(WARNING "[n8v] cppwinrt failed (${_result}): ${_error} - the WinUI 3 backend is disabled")
        set(_N8V_WINUI3_OK FALSE)
      endif()
    endif()
  endif()

  if(_N8V_WINUI3_OK)
    set(N8V_HAVE_WINUI3 TRUE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
      set(_rid "win-arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(_rid "win-x86")
    else()
      set(_rid "win-x64")
    endif()
    set(N8V_WINUI3_BOOTSTRAP_DLL "${_N8V_WINUI3_DIR}/pkg/microsoft.windowsappsdk.foundation/runtimes/${_rid}/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")
    set(N8V_WINUI3_INCLUDE_DIRS "${_N8V_WINUI3_GEN}" "${_N8V_WINUI3_DIR}/pkg/microsoft.windowsappsdk.interactiveexperiences/include")
  endif()
endif()
