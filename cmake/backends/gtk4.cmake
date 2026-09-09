option(N8V_BACKEND_GTK4 "Build the GTK4 native backend, if GTK4 is found." ON)

set(N8V_HAVE_GTK4 FALSE)

if(N8V_BACKEND_GTK4)
  cl_add_dep(GTK4)
  find_library(_N8V_GTK4_SO NAMES gtk-4)
  find_library(_N8V_GOBJECT_SO NAMES gobject-2.0)
  find_library(_N8V_GLIB_SO NAMES glib-2.0)

  if(TARGET deps::GTK4 AND _N8V_GTK4_SO AND _N8V_GOBJECT_SO AND _N8V_GLIB_SO)
    set(N8V_HAVE_GTK4 TRUE)

    get_target_property(_N8V_GTK4_INCLUDE_DIRS deps::GTK4 INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _N8V_GTK4_INCLUDE_DIRS)
      set(_N8V_GTK4_INCLUDE_DIRS "")
    endif()

    lzy_add_wrapper(gtk4_lazy
      LIBRARY_NAMES
        "libgtk-4.so.1"
        "libgtk-4.so"
        "libgtk-4.1.dylib"
        "libgtk-4.dylib"
        "gtk-4-1.dll"
      LIBRARY "${_N8V_GTK4_SO}"
    )
    target_include_directories(gtk4_lazy PUBLIC ${_N8V_GTK4_INCLUDE_DIRS})

    lzy_add_wrapper(gobject_lazy
      LIBRARY_NAMES
        "libgobject-2.0.so.0"
        "libgobject-2.0.so"
        "libgobject-2.0.0.dylib"
        "libgobject-2.0.dylib"
        "gobject-2.0-0.dll"
      SYMBOLS
        g_signal_connect_data
        g_object_unref
        g_object_ref_sink
        g_type_check_instance_cast
    )
    target_include_directories(gobject_lazy PUBLIC ${_N8V_GTK4_INCLUDE_DIRS})

    lzy_add_wrapper(glib_lazy
      LIBRARY_NAMES
        "libglib-2.0.so.0"
        "libglib-2.0.so"
        "libglib-2.0.0.dylib"
        "libglib-2.0.dylib"
        "glib-2.0-0.dll"
      SYMBOLS
        g_main_context_default
        g_main_context_iteration
        g_main_context_pending
        g_markup_escape_text
        g_free
        g_free_sized
    )
    target_include_directories(glib_lazy PUBLIC ${_N8V_GTK4_INCLUDE_DIRS})
  else()
    message(STATUS "[n8v] GTK4 not found - native_gtk4 backend disabled")
  endif()
endif()
