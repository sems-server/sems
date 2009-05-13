set (mod_utils_SRCS
ModUtils.cpp
)

ADD_LIBRARY(mod_utils SHARED ${mod_utils_SRCS})
SET_TARGET_PROPERTIES(mod_utils PROPERTIES OUTPUT_NAME "mod_utils")
SET_TARGET_PROPERTIES(mod_utils PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(mod_utils ${CMAKE_DL_LIBS} pthread m)

