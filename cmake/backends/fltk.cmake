option(N8V_BACKEND_FLTK "Build the FLTK native backend, if FLTK is found." ON)

set(N8V_HAVE_FLTK FALSE)

if(N8V_BACKEND_FLTK)
  find_library(_N8V_FLTK_SO NAMES fltk)
  find_path(_N8V_FLTK_INCLUDE_DIR NAMES FL/Fl.H)

  if(_N8V_FLTK_SO AND _N8V_FLTK_INCLUDE_DIR)
    set(N8V_HAVE_FLTK TRUE)

    lzy_add_wrapper(fltk_lazy
      LIBRARY_NAMES
        "libfltk.so.1.4"
        "libfltk.so"
        "libfltk.1.4.dylib"
        "libfltk.dylib"
        "fltk.dll"
      LIBRARY "${_N8V_FLTK_SO}"
      VARIABLES
        fl_graphics_driver Fl_Graphics_Driver*
    )
    target_include_directories(fltk_lazy PUBLIC "${_N8V_FLTK_INCLUDE_DIR}")
  else()
    message(STATUS "[n8v] FLTK not found - native_fltk backend disabled")
  endif()
endif()
