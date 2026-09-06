cmake_minimum_required(VERSION 3.18)
project(n8v VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

add_library(n8v
  source/core/clay_impl.cpp
  source/core/open_url.cpp
  source/core/ui_core.cpp
  source/styles/plain/plain_style.cpp
  source/backends/clay_fallback/backend_select.cpp
  source/backends/clay_fallback/text/font_atlas.cpp
  source/backends/clay_fallback/text/font_resolve.cpp
  source/backends/clay_fallback/text/line_layout.cpp
  source/backends/clay_fallback/renderers/sdl2/sdl2_backend.cpp
)
target_include_directories(n8v PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(n8v PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(n8v PRIVATE sdl2_lazy)
cl_add_dep(n8v clay PUBLIC)
cl_add_dep(n8v stb_truetype PRIVATE)

option(N8V_EXAMPLES "Build the n8v example apps." ${PROJECT_IS_TOP_LEVEL})
if(N8V_EXAMPLES)
  add_subdirectory(examples/basic)
endif()
