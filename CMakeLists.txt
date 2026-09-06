cmake_minimum_required(VERSION 3.18)
project(n8v VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # Fixes CMakeRC

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cl-bootstrap.cmake)
cl_repo()

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
)
target_include_directories(sdl2_lazy PUBLIC ${_N8V_SDL2_INCLUDE_DIRS})

cl_import_source(
  NAME cmrc
  DOWNLOAD_ONLY
  URL https://github.com/vector-of-bool/cmrc/archive/refs/tags/2.0.1.tar.gz
)
include("${CL_SOURCE_DIR}/CMakeRC.cmake")

cl_import_source(
  NAME dejavu_fonts
  DOWNLOAD_ONLY
  URL https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-fonts-ttf-2.37.tar.bz2
)
cmrc_add_resource_library(n8v_fonts
  NAMESPACE n8v_fonts
  WHENCE "${CL_SOURCE_DIR}/ttf"
  "${CL_SOURCE_DIR}/ttf/DejaVuSans.ttf"
  "${CL_SOURCE_DIR}/ttf/DejaVuSans-Bold.ttf"
  "${CL_SOURCE_DIR}/ttf/DejaVuSans-Oblique.ttf"
  "${CL_SOURCE_DIR}/ttf/DejaVuSans-BoldOblique.ttf"
)

add_library(n8v
  source/core/clay_impl.cpp
  source/core/open_url.cpp
  source/core/ui_core.cpp
  source/styles/plain/plain_style.cpp
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

option(N8V_EXAMPLES "Build the n8v example apps." ${PROJECT_IS_TOP_LEVEL})
if(N8V_EXAMPLES)
  add_subdirectory(examples/basic)
endif()
