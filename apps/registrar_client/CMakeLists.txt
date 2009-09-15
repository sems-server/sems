set (registrar_client_SRCS
ContactInfo.cpp
SIPRegistrarClient.cpp
)

ADD_LIBRARY(registrar_client SHARED ${registrar_client_SRCS})
SET_TARGET_PROPERTIES(registrar_client PROPERTIES OUTPUT_NAME "registrar_client")
TARGET_LINK_LIBRARIES(registrar_client ${CMAKE_DL_LIBS} pthread m)

