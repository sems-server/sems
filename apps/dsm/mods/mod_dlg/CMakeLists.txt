set (mod_dlg_SRCS
ModDlg.cpp
)

add_library(mod_dlg SHARED ${mod_dlg_SRCS})
set_target_properties(mod_dlg PROPERTIES OUTPUT_NAME "mod_dlg")
set_target_properties(mod_dlg PROPERTIES PREFIX "")
target_link_libraries(mod_dlg ${CMAKE_DL_LIBS} pthread m)

