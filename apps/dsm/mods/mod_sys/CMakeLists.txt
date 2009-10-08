set (mod_sys_SRCS
ModSys.cpp
)

ADD_LIBRARY(mod_sys SHARED ${mod_sys_SRCS})
TARGET_LINK_LIBRARIES(mod_sys ${CMAKE_DL_LIBS})

