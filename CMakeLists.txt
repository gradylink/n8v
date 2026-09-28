cmake_minimum_required(VERSION 3.18)
project(n8v VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CL_DISABLE_system_clay TRUE CACHE BOOL "" FORCE)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cl-bootstrap.cmake)
cl_repo()
cl_repo("${CMAKE_CURRENT_SOURCE_DIR}/cmake/recipes")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/lzy-bootstrap.cmake)

function(n8v_restrict_to_build_interface TARGET_NAME)
  get_target_property(_n8v_rtbi_includes ${TARGET_NAME} INTERFACE_INCLUDE_DIRECTORIES)
  if(NOT _n8v_rtbi_includes)
    return()
  endif()
  set(_n8v_rtbi_wrapped "")
  foreach(_n8v_rtbi_dir ${_n8v_rtbi_includes})
    list(APPEND _n8v_rtbi_wrapped "$<BUILD_INTERFACE:${_n8v_rtbi_dir}>")
  endforeach()
  set_target_properties(${TARGET_NAME} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_n8v_rtbi_wrapped}")
endfunction()

function(n8v_export_dependency_closure TARGET_NAME)
  if(NOT TARGET ${TARGET_NAME})
    return()
  endif()
  get_property(_n8v_visited GLOBAL PROPERTY "_N8V_EXPORT_VISITED_${TARGET_NAME}")
  if(_n8v_visited)
    return()
  endif()
  set_property(GLOBAL PROPERTY "_N8V_EXPORT_VISITED_${TARGET_NAME}" TRUE)

  get_target_property(_n8v_alias_of ${TARGET_NAME} ALIASED_TARGET)
  if(_n8v_alias_of)
    n8v_export_dependency_closure(${_n8v_alias_of})
    return()
  endif()

  get_target_property(_n8v_imported ${TARGET_NAME} IMPORTED)
  if(_n8v_imported)
    set_property(GLOBAL APPEND PROPERTY N8V_EXTERNAL_LINK_TARGETS "${TARGET_NAME}")
    if(TARGET_NAME MATCHES "^PkgConfig::(.+)$")
      set_property(GLOBAL APPEND PROPERTY N8V_FIND_PKGCONFIG_MODULES "${CMAKE_MATCH_1}")
    else()
      string(REGEX REPLACE "::.*" "" _n8v_pkg_guess "${TARGET_NAME}")
      set_property(GLOBAL APPEND PROPERTY N8V_FIND_DEPENDENCY_PACKAGES "${_n8v_pkg_guess}")
    endif()
    return()
  endif()

  n8v_restrict_to_build_interface(${TARGET_NAME})
  install(TARGETS ${TARGET_NAME} EXPORT n8vTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  set_property(GLOBAL APPEND PROPERTY N8V_EXPORTED_DEP_TARGETS "n8v::${TARGET_NAME}")

  foreach(_n8v_prop INTERFACE_LINK_LIBRARIES LINK_LIBRARIES)
    get_target_property(_n8v_libs ${TARGET_NAME} ${_n8v_prop})
    if(_n8v_libs)
      foreach(_n8v_lib ${_n8v_libs})
        if(TARGET ${_n8v_lib})
          n8v_export_dependency_closure(${_n8v_lib})
        endif()
      endforeach()
    endif()
    unset(_n8v_libs)
  endforeach()
endfunction()

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
      SDL_RenderCopy
      SDL_RenderGeometry
      SDL_RenderDrawLine
      SDL_RenderSetClipRect
      SDL_IntersectRect
      SDL_CreateSystemCursor
      SDL_SetCursor
      SDL_FreeCursor
      SDL_StartTextInput
      SDL_StopTextInput
      SDL_GetClipboardText
      SDL_SetClipboardText
      SDL_free
      SDL_GetTicks
      SDL_SetTextureAlphaMod
      SDL_SetTextureColorMod
  )
  target_include_directories(sdl2_lazy PUBLIC ${_N8V_SDL2_INCLUDE_DIRS})
endif()

cl_import_source(
  NAME bundle_cmake
  DOWNLOAD_ONLY
  REPO https://github.com/gradylink/bundle.cmake.git
  REF 50814e22ed67c0069ed9826f84a6817e0fd56c2b
)
include("${CL_SOURCE_DIR}/cmake/bundle.cmake")

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

bundle_add(n8v_fonts
  BASE_DIR "${_N8V_FONT_STAGE}"
  DIRECTORIES "${_N8V_FONT_STAGE}"
)

# Fixes bundle_add recipe override
cl_repo("${CMAKE_CURRENT_SOURCE_DIR}/cmake/recipes")

set(_N8V_ICON_STAGE "${CMAKE_CURRENT_BINARY_DIR}/n8v_icons")

cl_import_source(
  NAME lucide_icons
  DOWNLOAD_ONLY
  REPO https://github.com/lucide-icons/lucide.git
  REF main
)
set(_N8V_PLAIN_ICONS
  "settings:settings" "search:search" "check:check" "close:x" "menu:menu"
  "add:plus" "remove:minus" "delete:trash" "edit:pencil" "copy:copy"
  "download:download" "upload:upload" "refresh:refresh-cw" "share:share-2"
  "star:star" "heart:heart" "home:house" "info:info" "warning:triangle-alert"
  "error:circle-alert" "notification:bell" "calendar:calendar" "clock:clock"
  "mail:mail" "folder:folder" "file:file" "image:image" "link:link" "lock:lock"
  "unlock:lock-open" "visibility:eye" "visibility-off:eye-off" "wifi:wifi"
  "volume:volume-2" "mute:volume-x" "chevron-left:chevron-left" "chevron-right:chevron-right"
  "chevron-up:chevron-up" "chevron-down:chevron-down" "arrow-left:arrow-left"
  "arrow-right:arrow-right" "arrow-up:arrow-up" "arrow-down:arrow-down"
  "external-link:external-link" "user:user"
)
foreach(_pair ${_N8V_PLAIN_ICONS})
  string(REPLACE ":" ";" _pair "${_pair}")
  list(GET _pair 0 _name)
  list(GET _pair 1 _src)
  configure_file("${CL_SOURCE_DIR}/icons/${_src}.svg" "${_N8V_ICON_STAGE}/plain/${_name}.svg" COPYONLY)
endforeach()

cl_import_source(
  NAME tabler_icons
  DOWNLOAD_ONLY
  REPO https://github.com/tabler/tabler-icons.git
  REF main
)
set(_N8V_CUPERTINO_ICONS
  "settings:settings" "search:search" "check:check" "close:x" "menu:menu"
  "add:plus" "remove:minus" "delete:trash" "edit:pencil" "copy:copy"
  "download:download" "upload:upload" "refresh:refresh" "share:share-2"
  "star:star" "heart:heart" "home:home" "info:info-circle" "warning:alert-triangle"
  "error:alert-circle" "notification:bell" "calendar:calendar" "clock:clock"
  "mail:mail" "folder:folder" "file:file" "image:photo" "link:link" "lock:lock"
  "unlock:lock-open" "visibility:eye" "visibility-off:eye-off" "wifi:wifi"
  "volume:volume-2" "mute:volume-off" "chevron-left:chevron-left" "chevron-right:chevron-right"
  "chevron-up:chevron-up" "chevron-down:chevron-down" "arrow-left:arrow-left"
  "arrow-right:arrow-right" "arrow-up:arrow-up" "arrow-down:arrow-down"
  "external-link:external-link" "user:user"
)
foreach(_pair ${_N8V_CUPERTINO_ICONS})
  string(REPLACE ":" ";" _pair "${_pair}")
  list(GET _pair 0 _name)
  list(GET _pair 1 _src)
  configure_file("${CL_SOURCE_DIR}/icons/outline/${_src}.svg" "${_N8V_ICON_STAGE}/cupertino/${_name}.svg" COPYONLY)
endforeach()

cl_import_source(
  NAME material_icons
  DOWNLOAD_ONLY
  REPO https://github.com/google/material-design-icons.git
  REF master
)
set(_N8V_MATERIAL_ICONS
  "settings:settings" "search:search" "check:check" "close:close" "menu:menu"
  "add:add" "remove:remove" "delete:delete" "edit:edit" "copy:content_copy"
  "download:download" "upload:upload" "refresh:refresh" "share:share"
  "star:star" "heart:favorite" "home:home" "info:info" "warning:warning"
  "error:error" "notification:notifications" "calendar:calendar_today" "clock:schedule"
  "mail:mail" "folder:folder" "file:description" "image:image" "link:link" "lock:lock"
  "unlock:lock_open" "visibility:visibility" "visibility-off:visibility_off" "wifi:wifi"
  "volume:volume_up" "mute:volume_off" "chevron-left:chevron_left" "chevron-right:chevron_right"
  "chevron-up:expand_less" "chevron-down:expand_more" "arrow-left:arrow_back"
  "arrow-right:arrow_forward" "arrow-up:arrow_upward" "arrow-down:arrow_downward"
  "external-link:open_in_new" "user:person"
)
foreach(_pair ${_N8V_MATERIAL_ICONS})
  string(REPLACE ":" ";" _pair "${_pair}")
  list(GET _pair 0 _name)
  list(GET _pair 1 _src)
  configure_file(
    "${CL_SOURCE_DIR}/symbols/web/${_src}/materialsymbolsoutlined/${_src}_24px.svg"
    "${_N8V_ICON_STAGE}/material/${_name}.svg"
    COPYONLY
  )
endforeach()

cl_import_source(
  NAME fluent_icons
  DOWNLOAD_ONLY
  REPO https://github.com/microsoft/fluentui-system-icons.git
  REF main
)
set(_N8V_FLUENT_ICONS
  "settings:Settings:settings" "search:Search:search" "check:Checkmark:checkmark"
  "close:Dismiss:dismiss" "menu:Line Horizontal 3:line_horizontal_3" "add:Add:add"
  "remove:Subtract:subtract" "delete:Delete:delete" "edit:Edit:edit" "copy:Copy:copy"
  "download:Arrow Download:arrow_download" "upload:Arrow Upload:arrow_upload"
  "refresh:Arrow Sync:arrow_sync" "share:Share:share" "star:Star:star" "heart:Heart:heart"
  "home:Home:home" "info:Info:info" "warning:Warning:warning" "error:Error Circle:error_circle"
  "notification:Alert:alert" "calendar:Calendar:calendar" "clock:Clock:clock" "mail:Mail:mail"
  "folder:Folder:folder" "file:Document:document" "image:Image:image" "link:Link:link"
  "lock:Lock Closed:lock_closed" "unlock:Lock Open:lock_open" "visibility:Eye:eye"
  "visibility-off:Eye Off:eye_off" "wifi:WiFi 1:wifi_1" "volume:Speaker 2:speaker_2" "mute:Speaker Mute:speaker_mute"
  "chevron-left:Chevron Left:chevron_left" "chevron-right:Chevron Right:chevron_right"
  "chevron-up:Chevron Up:chevron_up" "chevron-down:Chevron Down:chevron_down"
  "arrow-left:Arrow Left:arrow_left" "arrow-right:Arrow Right:arrow_right"
  "arrow-up:Arrow Up:arrow_up" "arrow-down:Arrow Down:arrow_down" "external-link:Open:open"
  "user:Person:person"
)
foreach(_triple ${_N8V_FLUENT_ICONS})
  string(REPLACE ":" ";" _triple "${_triple}")
  list(GET _triple 0 _name)
  list(GET _triple 1 _dir)
  list(GET _triple 2 _src)
  configure_file(
    "${CL_SOURCE_DIR}/assets/${_dir}/SVG/ic_fluent_${_src}_24_regular.svg"
    "${_N8V_ICON_STAGE}/fluent/${_name}.svg"
    COPYONLY
  )
endforeach()

bundle_add(n8v_icons
  BASE_DIR "${_N8V_ICON_STAGE}"
  DIRECTORIES "${_N8V_ICON_STAGE}"
)

if(NOT EMSCRIPTEN)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/gtk4.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/qt.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/milsko.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/fltk.cmake")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/backends/html.cmake")

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  option(N8V_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" ON)
else()
  option(N8V_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
endif()

if(N8V_SANITIZE AND (N8V_HAVE_FLTK OR N8V_HAVE_QT))
  message(STATUS "[n8v] N8V_SANITIZE requested but FLTK/Qt are enabled (incompatible lzy RTTI shims) - skipping sanitizers")
  set(N8V_SANITIZE OFF)
endif()

if(N8V_SANITIZE)
  message(STATUS "[n8v] Sanitizers enabled (ASan + UBSan)")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

set(N8V_SOURCES
  source/core/clay_impl.cpp
  source/core/open_url.cpp
  source/core/ui_core.cpp
  source/core/ui_core_c_api.cpp
  source/core/n8v_ffi.c
  source/core/image_loader.cpp
  source/core/icon_registry.cpp
  source/core/icon_loader.cpp
  source/core/widgets/leaf.cpp
  source/core/widgets/checkbox.cpp
  source/core/widgets/radio.cpp
  source/core/widgets/toggle.cpp
  source/core/widgets/entry.cpp
  source/core/widgets/dropdown.cpp
  source/core/widgets/slider.cpp
  source/core/widgets/image.cpp
  source/core/widgets/icon.cpp
  source/core/widgets/sidebar.cpp
  source/core/widgets/panel.cpp
  source/styles/style_registry.cpp
  source/styles/plain/plain_style.cpp
  source/styles/material/material_style.cpp
  source/styles/cupertino/cupertino_style.cpp
  source/styles/fluent/fluent_style.cpp
  source/styles/custom/custom_style.cpp
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
    source/backends/clay_fallback/renderers/html/image_field.cpp
  )
else()
  list(APPEND N8V_SOURCES
    source/core/freedesktop_icon_theme.cpp
    source/backends/clay_fallback/renderers/sdl2/sdl2_backend.cpp
    source/backends/clay_fallback/renderers/sdl2/entry_field.cpp
    source/backends/clay_fallback/renderers/sdl2/text_selection.cpp
    source/backends/clay_fallback/renderers/sdl2/indicators.cpp
    source/backends/clay_fallback/renderers/sdl2/draw_primitives.cpp
    source/backends/clay_fallback/renderers/sdl2/image_field.cpp
  )
endif()

option(N8V_BUILD_STATIC "Build a shared libn8v.so." ON)
option(N8V_BUILD_SHARED "Build a shared libn8v.so." ON)
if(EMSCRIPTEN)
  set(N8V_BUILD_SHARED OFF)
  set(N8V_BUILD_STATIC OFF)
elseif(NOT N8V_BUILD_STATIC AND NOT N8V_BUILD_SHARED)
  message(FATAL_ERROR "You must enable either 'N8V_BUILD_STATIC' or 'N8V_BUILD_SHARED'.")
endif()

set(_n8v_targets "")
if(N8V_BUILD_STATIC)
  add_library(n8v STATIC ${N8V_SOURCES})
  list(APPEND _n8v_targets n8v)
endif()
if(N8V_BUILD_SHARED)
  add_library(n8v_shared SHARED ${N8V_SOURCES})
  set_target_properties(n8v_shared PROPERTIES OUTPUT_NAME n8v)
  list(APPEND _n8v_targets n8v_shared)
  if(NOT APPLE)
    target_link_options(n8v_shared PRIVATE "LINKER:--exclude-libs,ALL")
  endif()
endif()

foreach(_n8v_target ${_n8v_targets})
  set_target_properties(${_n8v_target} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
  target_include_directories(${_n8v_target} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  target_include_directories(${_n8v_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
  target_link_libraries(${_n8v_target} PUBLIC n8v_fonts n8v_icons)
  cl_add_dep(${_n8v_target} plutosvg PUBLIC)
  cl_add_dep(${_n8v_target} clay PRIVATE)
  cl_add_dep(${_n8v_target} stb_truetype PRIVATE)
  cl_add_dep(${_n8v_target} stb_image PRIVATE)

  if(NOT EMSCRIPTEN)
    target_link_libraries(${_n8v_target} PUBLIC sdl2_lazy)
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
    target_link_libraries(${_n8v_target} PUBLIC gtk4_lazy gobject_lazy glib_lazy)
    if(N8V_HAVE_ADWAITA)
      target_link_libraries(${_n8v_target} PUBLIC adwaita_lazy)
      target_compile_definitions(${_n8v_target} PRIVATE N8V_HAVE_ADWAITA)
    endif()
  endif()

  if(N8V_HAVE_QT)
    target_sources(${_n8v_target} PRIVATE source/backends/native_qt/qt_backend.cpp)
    target_compile_definitions(${_n8v_target} PRIVATE N8V_HAS_QT_BACKEND)
    target_link_libraries(${_n8v_target} PUBLIC qt6widgets_lazy qt6gui_lazy qt6core_lazy)
  endif()

  if(N8V_HAVE_MILSKO)
    target_sources(${_n8v_target} PRIVATE source/backends/native_milsko/milsko_backend.cpp)
    target_compile_definitions(${_n8v_target} PRIVATE N8V_HAS_MILSKO_BACKEND)
    target_link_libraries(${_n8v_target} PUBLIC milsko_lazy)
  endif()

  if(N8V_HAVE_FLTK)
    target_sources(${_n8v_target} PRIVATE source/backends/native_fltk/fltk_backend.cpp)
    target_compile_definitions(${_n8v_target} PRIVATE N8V_HAS_FLTK_BACKEND)
    target_link_libraries(${_n8v_target} PUBLIC fltk_lazy)
  endif()
endforeach()

option(N8V_INSTALL "Install n8v and generate its CMake package config + pkg-config files." ${PROJECT_IS_TOP_LEVEL})
if(N8V_INSTALL AND NOT EMSCRIPTEN)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  if(N8V_BUILD_STATIC)
    install(TARGETS n8v
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
  endif()
  if(N8V_BUILD_SHARED)
    install(TARGETS n8v_shared
      EXPORT n8vTargets
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
  endif()

  set(_n8v_lazy_export_targets sdl2_lazy)
  if(N8V_HAVE_GTK4)
    list(APPEND _n8v_lazy_export_targets gtk4_lazy gobject_lazy glib_lazy)
    if(N8V_HAVE_ADWAITA)
      list(APPEND _n8v_lazy_export_targets adwaita_lazy)
    endif()
  endif()
  if(N8V_HAVE_QT)
    list(APPEND _n8v_lazy_export_targets qt6widgets_lazy qt6gui_lazy qt6core_lazy)
  endif()
  if(N8V_HAVE_MILSKO)
    list(APPEND _n8v_lazy_export_targets milsko_lazy)
  endif()
  if(N8V_HAVE_FLTK)
    list(APPEND _n8v_lazy_export_targets fltk_lazy)
  endif()
  foreach(_n8v_lazy_target ${_n8v_lazy_export_targets})
    n8v_restrict_to_build_interface(${_n8v_lazy_target})
    install(TARGETS ${_n8v_lazy_target}
      EXPORT n8vTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
  endforeach()

  if(N8V_BUILD_STATIC)
    n8v_export_dependency_closure(n8v_fonts)
    n8v_export_dependency_closure(n8v_icons)
    n8v_export_dependency_closure(deps::plutosvg)
  endif()
  get_property(_N8V_FIND_DEPENDENCY_PACKAGES GLOBAL PROPERTY N8V_FIND_DEPENDENCY_PACKAGES)
  get_property(_N8V_FIND_PKGCONFIG_MODULES GLOBAL PROPERTY N8V_FIND_PKGCONFIG_MODULES)
  get_property(_N8V_EXPORTED_DEP_TARGETS GLOBAL PROPERTY N8V_EXPORTED_DEP_TARGETS)
  get_property(_N8V_EXTERNAL_LINK_TARGETS GLOBAL PROPERTY N8V_EXTERNAL_LINK_TARGETS)
  list(REMOVE_DUPLICATES _N8V_FIND_DEPENDENCY_PACKAGES)
  list(REMOVE_DUPLICATES _N8V_FIND_PKGCONFIG_MODULES)

  install(EXPORT n8vTargets
    FILE n8vTargets.cmake
    NAMESPACE n8v::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/n8v
  )

  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/packaging/n8vConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/n8vConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/n8v
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/n8vConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/n8vConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/n8vConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/n8v
  )

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/packaging/n8v.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/n8v.pc
    @ONLY
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/n8v.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

option(N8V_EXAMPLES "Build the n8v example apps." ${PROJECT_IS_TOP_LEVEL})
if(N8V_EXAMPLES)
  add_subdirectory(examples/basic)
  add_subdirectory(examples/basic_c)
  add_subdirectory(examples/custom_style)
endif()

option(N8V_BUILD_JS_BINDING "Build the n8v.node N-API addon for the JS/TS bindings (bindings/js)." OFF)
if(N8V_BUILD_JS_BINDING)
  add_subdirectory(bindings/js/native)
endif()
