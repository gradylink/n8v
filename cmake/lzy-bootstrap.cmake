if(DEFINED __LZY_BOOTSTRAPPED)
  return()
endif()
set(__LZY_BOOTSTRAPPED TRUE)

cl_import_source(
  NAME lzy_cmake
  DOWNLOAD_ONLY
  REPO https://github.com/gradylink/lzy.cmake.git
  REF 50a19e6c9bb146f080d3add8fded5b69929be24c
)

include("${CL_SOURCE_DIR}/cmake/lzy.cmake")
