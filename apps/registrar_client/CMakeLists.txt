set (registrar_client_SRCS
ContactInfo.cpp
SIPRegistrarClient.cpp
)

ADD_LIBRARY(registrar_client SHARED ${registrar_client_SRCS})
TARGET_LINK_LIBRARIES(registrar_client ${CMAKE_DL_LIBS})

