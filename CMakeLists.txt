cmake_minimum_required(VERSION 3.18)
project(n8v VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cl-bootstrap.cmake)
cl_repo()
cl_repo("${CMAKE_CURRENT_SOURCE_DIR}/cmake/recipes")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/lzy-bootstrap.cmake)

if(NOT EMSCRIPTEN)
  if(NOT DEFINED _N8V_SDL2_INCLUDE_DIRS)
    cl_add_dep(SDL2)
    get_target_property(_N8V_SDL2_INCLUDE_DIRS deps::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _N8V_SDL2_INCLUDE_DIRS)
      set(_N8V_SDL2_INCLUDE_DIRS "")
    endif()
  endif()

  lzy_add_wrapper(sdl2_lazy
    LIBRARY_NAMES
      "libSDL2-2.0.so.0"
      "libSDL2-2.0.so"
      "libSDL2.so"
      "libSDL2.dylib"
      "SDL2.dll"
    SYMBOLS
      SDL_Init
      SDL_Quit
      SDL_GetError
      SDL_CreateWindow
      SDL_DestroyWindow
      SDL_CreateRenderer
      SDL_DestroyRenderer
      SDL_GL_SetAttribute
      SDL_SetRenderDrawColor
      SDL_SetRenderDrawBlendMode
      SDL_RenderClear
      SDL_RenderFillRect
      SDL_RenderPresent
      SDL_PollEvent
      SDL_GetWindowSize
      SDL_CreateTexture
      SDL_DestroyTexture
      SDL_SetTextureBlendMode
      SDL_UpdateTexture
      SDL_RenderGeometry
      SDL_RenderDrawLine
      SDL_CreateSystemCursor
      SDL_SetCursor
      SDL_FreeCursor
      SDL_StartTextInput
      SDL_StopTextInput
      SDL_GetClipboardText
      SDL_SetClipboardText
      SDL_free
      SDL_GetTicks
  )
  target_include_directories(sdl2_lazy PUBLIC ${_N8V_SDL2_INCLUDE_DIRS})
endif()

cl_import_source(
  NAME cmrc
  DOWNLOAD_ONLY
  REPO https://github.com/vector-of-bool/cmrc.git
  REF 952ffddba731fc110bd50409e8d2b8a06abbd237
)
include("${CL_SOURCE_DIR}/CMakeRC.cmake")

set(_N8V_FONT_STAGE "${CMAKE_CURRENT_BINARY_DIR}/n8v_fonts")

cl_import_source(
  NAME dejavu_fonts
  DOWNLOAD_ONLY
  URL https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-fonts-ttf-2.37.tar.bz2
)
foreach(_face DejaVuSans DejaVuSans-Bold DejaVuSans-Oblique DejaVuSans-BoldOblique)
  configure_file("${CL_SOURCE_DIR}/ttf/${_face}.ttf" "${_N8V_FONT_STAGE}/${_face}.ttf" COPYONLY)
endforeach()

cl_import_source(
  NAME roboto_fonts
  DOWNLOAD_ONLY
  URL https://github.com/googlefonts/roboto/releases/download/v2.138/roboto-unhinted.zip
)
foreach(_face Roboto-Regular Roboto-Bold Roboto-Italic Roboto-BoldItalic)
  configure_file("${CL_SOURCE_DIR}/${_face}.ttf" "${_N8V_FONT_STAGE}/${_face}.ttf" COPYONLY)
endforeach()

cl_import_source(
  NAME inter_fonts
  DOWNLOAD_ONLY
  URL https://github.com/rsms/inter/releases/download/v3.19/Inter-3.19.zip
)
foreach(_face Inter-Regular Inter-Bold Inter-Italic Inter-BoldItalic)
  configure_file("${CL_SOURCE_DIR}/Inter Hinted for Windows/Desktop/${_face}.ttf" "${_N8V_FONT_STAGE}/${_face}.ttf" COPYONLY)
endforeach()

cl_import_source(
  NAME selawik_fonts
  DOWNLOAD_ONLY
  URL https://github.com/microsoft/Selawik/releases/download/1.01/Selawik_Release.zip
)
configure_file("${CL_SOURCE_DIR}/selawk.ttf" "${_N8V_FONT_STAGE}/Selawik-Regular.ttf" COPYONLY)
configure_file("${CL_SOURCE_DIR}/selawkb.ttf" "${_N8V_FONT_STAGE}/Selawik-Bold.ttf" COPYONLY)

file(GLOB _N8V_FONT_FILES "${_N8V_FONT_STAGE}/*.ttf")
cmrc_add_resource_library(n8v_fonts
  NAMESPACE n8v_fonts
  WHENCE "${_N8V_FONT_STAGE}"
  ${_N8V_FONT_FILES}
)

if(NOT EMSCRIPTEN)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/gtk4.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/qt.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/milsko.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/fltk.cmake")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/html.cmake")

set(N8V_SOURCES
  source/core/clay_impl.cpp
  source/core/open_url.cpp
  source/core/ui_core.cpp
  source/core/ui_core_c_api.cpp
  source/core/widgets/leaf.cpp
  source/core/widgets/checkbox.cpp
  source/core/widgets/radio.cpp
  source/core/widgets/entry.cpp
  source/core/widgets/dropdown.cpp
  source/core/widgets/slider.cpp
  source/styles/style_registry.cpp
  source/styles/plain/plain_style.cpp
  source/styles/material/material_style.cpp
  source/styles/cupertino/cupertino_style.cpp
  source/styles/fluent/fluent_style.cpp
  source/backends/clay_fallback/backend_select.cpp
  source/backends/clay_fallback/text/bundled_font.cpp
  source/backends/clay_fallback/text/font_atlas.cpp
  source/backends/clay_fallback/text/line_layout.cpp
)

if(EMSCRIPTEN)
  list(APPEND N8V_SOURCES
    source/backends/clay_fallback/renderers/html/html_backend.cpp
    source/backends/clay_fallback/renderers/html/dom_commands.cpp
    source/backends/clay_fallback/renderers/html/indicators.cpp
    source/backends/clay_fallback/renderers/html/entry_field.cpp
    source/backends/clay_fallback/renderers/html/fonts.cpp
    source/backends/clay_fallback/renderers/html/text_measure.cpp
    source/backends/clay_fallback/renderers/html/js_bridge.cpp
  )
else()
  list(APPEND N8V_SOURCES
    source/backends/clay_fallback/renderers/sdl2/sdl2_backend.cpp
    source/backends/clay_fallback/renderers/sdl2/entry_field.cpp
    source/backends/clay_fallback/renderers/sdl2/text_selection.cpp
    source/backends/clay_fallback/renderers/sdl2/indicators.cpp
    source/backends/clay_fallback/renderers/sdl2/draw_primitives.cpp
  )
endif()

option(N8V_BUILD_SHARED "Also build a shared libn8v.so." ON)
if(EMSCRIPTEN)
  set(N8V_BUILD_SHARED OFF)
endif()

set(_n8v_targets n8v)
add_library(n8v STATIC ${N8V_SOURCES})
if(N8V_BUILD_SHARED)
  add_library(n8v_shared SHARED ${N8V_SOURCES})
  set_target_properties(n8v_shared PROPERTIES OUTPUT_NAME n8v)
  list(APPEND _n8v_targets n8v_shared)
  target_link_options(n8v_shared PRIVATE "LINKER:--exclude-libs,ALL")
endif()

foreach(_n8v_target ${_n8v_targets})
  set_target_properties(${_n8v_target} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
  target_include_directories(${_n8v_target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_include_directories(${_n8v_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
  target_link_libraries(${_n8v_target} PRIVATE n8v_fonts)
  cl_add_dep(${_n8v_target} clay PUBLIC)
  cl_add_dep(${_n8v_target} stb_truetype PRIVATE)

  if(NOT EMSCRIPTEN)
    target_link_libraries(${_n8v_target} PRIVATE sdl2_lazy)
  endif()

  if(N8V_HAVE_HTML)
    target_link_options(${_n8v_target} INTERFACE
      "SHELL:-sASYNCIFY=1"
      "SHELL:-sALLOW_MEMORY_GROWTH=1"
      "SHELL:-sEXPORTED_RUNTIME_METHODS=[\"ccall\"]"
      "--bind"
      "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/source/backends/clay_fallback/renderers/html/shell_minimal.html"
    )
  endif()

  if(N8V_HAVE_GTK4)
    target_sources(${_n8v_target} PRIVATE source/backends/native_gtk4/gtk4_backend.cpp)
    target_compile_definitions(${_n8v_target} PRIVATE N8V_HAS_GTK4_BACKEND)
    target_link_libraries(${_n8v_target} PRIVATE gtk4_lazy gobject_lazy glib_lazy)
  endif()

  if(N8V_HAVE_QT)
    target_sources(${_n8v_target} PRIVATE source/backends/native_qt/qt_backend.cpp)
    target_compile_definitions(${_n8v_target} PRIVATE N8V_HAS_QT_BACKEND)
    target_link_libraries(${_n8v_target} PRIVATE qt6widgets_lazy qt6gui_lazy qt6core_lazy)
  endif()

  if(N8V_HAVE_MILSKO)
    target_sources(${_n8v_target} PRIVATE source/backends/native_milsko/milsko_backend.cpp)
    target_compile_definitions(${_n8v_target} PRIVATE N8V_HAS_MILSKO_BACKEND)
    target_link_libraries(${_n8v_target} PRIVATE milsko_lazy)
  endif()

  if(N8V_HAVE_FLTK)
    target_sources(${_n8v_target} PRIVATE source/backends/native_fltk/fltk_backend.cpp)
    target_compile_definitions(${_n8v_target} PRIVATE N8V_HAS_FLTK_BACKEND)
    target_link_libraries(${_n8v_target} PRIVATE fltk_lazy)
  endif()
endforeach()

option(N8V_EXAMPLES "Build the n8v example apps." ${PROJECT_IS_TOP_LEVEL})
if(N8V_EXAMPLES)
  add_subdirectory(examples/basic)
  add_subdirectory(examples/basic_c)
endif()
