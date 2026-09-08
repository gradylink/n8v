option(N8V_BACKEND_QT "Build the Qt native backend, if Qt6 Widgets is found." ON)

set(N8V_HAVE_QT FALSE)

if(N8V_BACKEND_QT)
  find_package(Qt6 QUIET COMPONENTS Widgets Gui Core)
  find_library(_N8V_QT6WIDGETS_SO NAMES Qt6Widgets)
  find_library(_N8V_QT6GUI_SO NAMES Qt6Gui)
  find_library(_N8V_QT6CORE_SO NAMES Qt6Core)

  if(Qt6Widgets_FOUND AND _N8V_QT6WIDGETS_SO AND _N8V_QT6GUI_SO AND _N8V_QT6CORE_SO)
    set(N8V_HAVE_QT TRUE)

    get_target_property(_N8V_QT6WIDGETS_INCLUDE_DIRS Qt6::Widgets INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(_N8V_QT6GUI_INCLUDE_DIRS Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(_N8V_QT6CORE_INCLUDE_DIRS Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)

    lzy_add_wrapper(qt6widgets_lazy
      LIBRARY_NAMES
        "libQt6Widgets.so.6"
        "libQt6Widgets.so"
        "libQt6Widgets.6.dylib"
        "libQt6Widgets.dylib"
        "Qt6Widgets.dll"
      LIBRARY "${_N8V_QT6WIDGETS_SO}"
      RTTI_SHIMS QPushButton QLabel QCheckBox QRadioButton
    )
    target_include_directories(qt6widgets_lazy PUBLIC ${_N8V_QT6WIDGETS_INCLUDE_DIRS})
    target_compile_definitions(qt6widgets_lazy PUBLIC QT_WIDGETS_LIB QT_GUI_LIB QT_CORE_LIB)

    lzy_add_wrapper(qt6gui_lazy
      LIBRARY_NAMES
        "libQt6Gui.so.6"
        "libQt6Gui.so"
        "libQt6Gui.6.dylib"
        "libQt6Gui.dylib"
        "Qt6Gui.dll"
      LIBRARY "${_N8V_QT6GUI_SO}"
    )
    target_include_directories(qt6gui_lazy PUBLIC ${_N8V_QT6GUI_INCLUDE_DIRS})

    lzy_add_wrapper(qt6core_lazy
      LIBRARY_NAMES
        "libQt6Core.so.6"
        "libQt6Core.so"
        "libQt6Core.6.dylib"
        "libQt6Core.dylib"
        "Qt6Core.dll"
      LIBRARY "${_N8V_QT6CORE_SO}"
    )
    target_include_directories(qt6core_lazy PUBLIC ${_N8V_QT6CORE_INCLUDE_DIRS})
  else()
    message(STATUS "[n8v] Qt6 Widgets not found - native_qt backend disabled")
  endif()
endif()
