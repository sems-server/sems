set (mod_dlg_SRCS
ModDlg.cpp
)

ADD_LIBRARY(mod_dlg SHARED ${mod_dlg_SRCS})
SET_TARGET_PROPERTIES(mod_dlg PROPERTIES OUTPUT_NAME "mod_dlg")
TARGET_LINK_LIBRARIES(mod_dlg ${CMAKE_DL_LIBS} pthread m)

