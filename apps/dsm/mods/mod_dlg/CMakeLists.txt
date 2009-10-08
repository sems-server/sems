set (mod_dlg_SRCS
ModDlg.cpp
)

ADD_LIBRARY(mod_dlg SHARED ${mod_dlg_SRCS})
TARGET_LINK_LIBRARIES(mod_dlg ${CMAKE_DL_LIBS})

