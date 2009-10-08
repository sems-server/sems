set (mod_utils_SRCS
ModUtils.cpp
)

ADD_LIBRARY(mod_utils SHARED ${mod_utils_SRCS})
TARGET_LINK_LIBRARIES(mod_utils ${CMAKE_DL_LIBS})

