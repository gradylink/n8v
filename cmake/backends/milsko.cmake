option(N8V_BACKEND_MILSKO "Build the Milsko native backend, if Milsko is found." ON)

set(N8V_HAVE_MILSKO FALSE)

if(N8V_BACKEND_MILSKO)
  find_library(_N8V_MILSKO_SO NAMES Mw)
  find_path(_N8V_MILSKO_INCLUDE_DIR NAMES Mw/Milsko.h)

  if(_N8V_MILSKO_SO AND _N8V_MILSKO_INCLUDE_DIR)
    set(N8V_HAVE_MILSKO TRUE)

    lzy_add_wrapper(milsko_lazy
      LIBRARY_NAMES
        "${_N8V_MILSKO_SO}"
        "libMw.so"
        "libMw.so.1"
        "Mw.dylib"
        "libMw.dylib"
        "Mw.dll"
      LIBRARY "${_N8V_MILSKO_SO}"
      VARIABLES
        MwWindowClass MwClass
        MwButtonClass MwClass
        MwLabelClass MwClass
        MwCheckBoxClass MwClass
        MwRadioBoxClass MwClass
        MwEntryClass MwClass
        MwComboBoxClass MwClass
        MwScrollBarClass MwClass
        MwImageClass MwClass
        MwLLDestroyPixmap MwLLDestroyPixmapFn
    )
    target_include_directories(milsko_lazy PUBLIC "${_N8V_MILSKO_INCLUDE_DIR}")
  else()
    message(STATUS "[n8v] Milsko not found - native_milsko backend disabled")
  endif()
endif()
