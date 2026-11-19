add_executable(n8v-static-site-example main.cpp)
if(TARGET n8v)
  target_link_libraries(n8v-static-site-example PRIVATE n8v)
else()
  target_link_libraries(n8v-static-site-example PRIVATE n8v_shared)
endif()

if(EMSCRIPTEN)
  set_target_properties(n8v-static-site-example PROPERTIES SUFFIX ".html")
else()
  set(_dist "${CMAKE_CURRENT_BINARY_DIR}/dist")
  add_custom_command(
    OUTPUT "${_dist}/index.html"
    COMMAND ${CMAKE_COMMAND} -E env N8V_BACKEND=static "N8V_STATIC_OUT=${_dist}" $<TARGET_FILE:n8v-static-site-example>
    DEPENDS n8v-static-site-example
    COMMENT "Rendering the static site example to ${_dist}"
  )
  add_custom_target(n8v-static-site-example-dist DEPENDS "${_dist}/index.html")
endif()
