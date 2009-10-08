set (mod_aws_SRCS
ModAws.cpp
)

ADD_LIBRARY(mod_aws SHARED ${mod_aws_SRCS})
TARGET_LINK_LIBRARIES(mod_aws ${CMAKE_DL_LIBS} aws)

