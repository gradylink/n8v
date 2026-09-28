function(_recipe_n8v_system)
  if(NOT CMAKE_CROSSCOMPILING)
    if(CL_REQ_VERSION)
      find_package(n8v ${CL_REQ_VERSION} CONFIG QUIET)
    else()
      find_package(n8v CONFIG QUIET)
    endif()
  endif()

  if(NOT TARGET n8v::n8v AND NOT TARGET n8v::n8v_shared)
    cl_format_pkgconfig_req("n8v" "${CL_VERSION_REQ}" PKG_SPEC)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(n8v IMPORTED_TARGET GLOBAL ${PKG_SPEC})
    endif()
  endif()
endfunction()

function(_recipe_n8v_source)
  set(N8V_BUILD_STATIC ON)
  set(N8V_BUILD_SHARED OFF)
  if(CL_REQ_TYPE STREQUAL "SHARED" OR CL_REQ_TYPE STREQUAL "PREFER_SHARED")
    set(N8V_BUILD_SHARED ON)
    set(N8V_BUILD_STATIC OFF)
  endif()
  
  cl_import_source(
    NAME n8v
    REPO https://github.com/gradylink/n8v.git
    OPTIONS "N8V_BUILD_SHARED" ${N8V_BUILD_SHARED} "N8V_BUILD_STATIC" ${N8V_BUILD_STATIC}
  )

  if(TARGET n8v)
    add_library(deps::n8v ALIAS n8v)
  else()
    add_library(deps::n8v ALIAS n8v_shared)
  endif()
endfunction()
