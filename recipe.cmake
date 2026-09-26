function(_recipe_n8v_source)
  set(N8V_BUILD_SHARED OFF)
  if(CL_REQ_TYPE STREQUAL "SHARED" OR CL_REQ_TYPE STREQUAL "PREFER_SHARED")
    set(N8V_BUILD_SHARED ON)
  endif()
  
  cl_import_source(
    NAME n8v
    REPO https://github.com/gradylink/n8v.git
    OPTIONS "N8V_BUILD_SHARED" ${N8V_BUILD_SHARED}
  )

  if(CL_REQ_TYPE STREQUAL "SHARED" OR CL_REQ_TYPE STREQUAL "PREFER_SHARED")
    add_library(deps::n8v ALIAS n8v_shared)
  else()
    add_library(deps::n8v ALIAS n8v)
  endif()
endfunction()
