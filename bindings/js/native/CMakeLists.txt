cl_import_source(
  NAME node_api_headers
  DOWNLOAD_ONLY
  REPO https://github.com/nodejs/node-api-headers.git
  REF main
)
set(_N8V_NODE_API_HEADERS_DIR ${CL_SOURCE_DIR})

cl_import_source(
  NAME node_addon_api
  DOWNLOAD_ONLY
  REPO https://github.com/nodejs/node-addon-api.git
  REF main
)
set(_N8V_NODE_ADDON_API_DIR ${CL_SOURCE_DIR})

add_library(n8v_napi MODULE
  addon.cpp
  marshal.cpp
)

set_target_properties(n8v_napi PROPERTIES
  PREFIX ""
  OUTPUT_NAME "n8v"
  SUFFIX ".node"
)

target_include_directories(n8v_napi PRIVATE
  ${_N8V_NODE_ADDON_API_DIR}
  ${_N8V_NODE_API_HEADERS_DIR}/include
)

target_compile_definitions(n8v_napi PRIVATE NAPI_VERSION=8 NODE_ADDON_API_DISABLE_DEPRECATED)
target_link_libraries(n8v_napi PRIVATE n8v)
set_target_properties(n8v_napi PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

if(APPLE)
  target_link_options(n8v_napi PRIVATE "-undefined" "dynamic_lookup")
else()
  target_link_options(n8v_napi PRIVATE "LINKER:--exclude-libs,ALL")
endif()

set_target_properties(n8v_napi PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
