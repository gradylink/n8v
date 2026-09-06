if(DEFINED __LZY_BOOTSTRAPPED)
  return()
endif()
set(__LZY_BOOTSTRAPPED TRUE)

cl_import_source(
  NAME lzy_cmake
  DOWNLOAD_ONLY
  REPO https://github.com/gradylink/lzy.cmake.git
  REF 8ea1fd3b6acfe4cfb2f9a38dc7ea91cd77ae89da
)

include("${CL_SOURCE_DIR}/cmake/lzy.cmake")
