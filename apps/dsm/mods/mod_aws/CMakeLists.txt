set (mod_aws_SRCS
ModAws.cpp
)

ADD_LIBRARY(mod_aws SHARED ${mod_aws_SRCS})
SET_TARGET_PROPERTIES(mod_aws PROPERTIES OUTPUT_NAME "mod_aws")
TARGET_LINK_LIBRARIES(mod_aws ${CMAKE_DL_LIBS} aws)

