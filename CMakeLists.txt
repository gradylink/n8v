cmake_minimum_required(VERSION 3.18)
project(n8v VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # Fixes CMakeRC

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cl-bootstrap.cmake)
cl_repo()
cl_repo("${CMAKE_CURRENT_SOURCE_DIR}/cmake/recipes")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/lzy-bootstrap.cmake)

cl_add_dep(SDL2)
get_target_property(_N8V_SDL2_INCLUDE_DIRS deps::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
if(NOT _N8V_SDL2_INCLUDE_DIRS)
  set(_N8V_SDL2_INCLUDE_DIRS "")
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
    SDL_SetRenderDrawColor
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

cl_import_source(
  NAME cmrc
  DOWNLOAD_ONLY
  URL https://github.com/vector-of-bool/cmrc/archive/refs/tags/2.0.1.tar.gz
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

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/gtk4.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/qt.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/milsko.cmake")

add_library(n8v
  source/core/clay_impl.cpp
  source/core/open_url.cpp
  source/core/ui_core.cpp
  source/styles/style_registry.cpp
  source/styles/plain/plain_style.cpp
  source/styles/material/material_style.cpp
  source/styles/cupertino/cupertino_style.cpp
  source/styles/fluent/fluent_style.cpp
  source/backends/clay_fallback/backend_select.cpp
  source/backends/clay_fallback/text/bundled_font.cpp
  source/backends/clay_fallback/text/font_atlas.cpp
  source/backends/clay_fallback/text/line_layout.cpp
  source/backends/clay_fallback/renderers/sdl2/sdl2_backend.cpp
)
target_include_directories(n8v PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(n8v PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(n8v PRIVATE sdl2_lazy n8v_fonts)
cl_add_dep(n8v clay PUBLIC)
cl_add_dep(n8v stb_truetype PRIVATE)

if(N8V_HAVE_GTK4)
  target_sources(n8v PRIVATE source/backends/native_gtk4/gtk4_backend.cpp)
  target_compile_definitions(n8v PRIVATE N8V_HAS_GTK4_BACKEND)
  target_link_libraries(n8v PRIVATE gtk4_lazy gobject_lazy glib_lazy)
endif()

if(N8V_HAVE_QT)
  target_sources(n8v PRIVATE source/backends/native_qt/qt_backend.cpp)
  target_compile_definitions(n8v PRIVATE N8V_HAS_QT_BACKEND)
  target_link_libraries(n8v PRIVATE qt6widgets_lazy qt6gui_lazy qt6core_lazy)
endif()

if(N8V_HAVE_MILSKO)
  target_sources(n8v PRIVATE source/backends/native_milsko/milsko_backend.cpp)
  target_compile_definitions(n8v PRIVATE N8V_HAS_MILSKO_BACKEND)
  target_link_libraries(n8v PRIVATE milsko_lazy)
endif()

option(N8V_EXAMPLES "Build the n8v example apps." ${PROJECT_IS_TOP_LEVEL})
if(N8V_EXAMPLES)
  add_subdirectory(examples/basic)
endif()
