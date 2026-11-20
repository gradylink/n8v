option(N8V_BACKEND_FTXUI "Build the FTXUI terminal backend." ON)

set(N8V_HAVE_FTXUI FALSE)

if(N8V_BACKEND_FTXUI)
  set(N8V_HAVE_FTXUI TRUE)
endif()
