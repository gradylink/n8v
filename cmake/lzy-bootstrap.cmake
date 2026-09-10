if(DEFINED __LZY_BOOTSTRAPPED)
  return()
endif()
set(__LZY_BOOTSTRAPPED TRUE)

cl_import_source(
  NAME lzy_cmake
  DOWNLOAD_ONLY
  REPO https://github.com/gradylink/lzy.cmake.git
  REF e0c9599ca5860c8bf17313b99e3d6d3e6408d706
)

include("${CL_SOURCE_DIR}/cmake/lzy.cmake")
