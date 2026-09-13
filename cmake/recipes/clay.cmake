function(_recipe_clay_source)
  set(CLAY_REF "e6cc36941ab2af5d81107617039d6f527a1c660b")
  if(CL_REQ_VERSION)
    set(CLAY_REF "${CL_REQ_VERSION}")
  endif()

  cl_import_source(
    NAME clay
    DOWNLOAD_ONLY
    URL https://github.com/nicbarker/clay/archive/${CLAY_REF}.tar.gz
  )

  if(NOT EXISTS "${CL_SOURCE_DIR}/clay.h")
    _catalog_log(FATAL_ERROR "clay: clay.h not found under ${CL_SOURCE_DIR}")
  endif()

  add_library(clay INTERFACE)
  target_include_directories(clay INTERFACE $<BUILD_INTERFACE:${CL_SOURCE_DIR}>)
endfunction()
