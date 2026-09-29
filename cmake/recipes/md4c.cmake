function(_recipe_md4c_source)
  set(MD4C_REF "release-0.5.2")
  if(CL_REQ_VERSION)
    set(MD4C_REF "${CL_REQ_VERSION}")
  endif()

  cl_import_source(
    NAME md4c
    DOWNLOAD_ONLY
    URL https://github.com/mity/md4c/archive/refs/tags/${MD4C_REF}.tar.gz
  )

  if(NOT EXISTS "${CL_SOURCE_DIR}/src/md4c.h")
    _catalog_log(FATAL_ERROR "md4c: md4c.h not found under ${CL_SOURCE_DIR}/src")
  endif()

  add_library(md4c STATIC "${CL_SOURCE_DIR}/src/md4c.c")
  set_target_properties(md4c PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_include_directories(md4c PUBLIC $<BUILD_INTERFACE:${CL_SOURCE_DIR}/src>)
  target_compile_definitions(md4c PUBLIC md_parse=n8v_vendored_md_parse) # Avoid (potential) milsko conflict
endfunction()
