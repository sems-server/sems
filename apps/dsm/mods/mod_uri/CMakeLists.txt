set (mod_uri_SRCS
ModUri.cpp
)

ADD_LIBRARY(mod_uri SHARED ${mod_uri_SRCS})
TARGET_LINK_LIBRARIES(mod_uri ${CMAKE_DL_LIBS})

