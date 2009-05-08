set (registrar_client_SRCS
ContactInfo.cpp
SIPRegistrarClient.cpp
)

add_library(registrar_client SHARED ${registrar_client_SRCS})
set_target_properties(registrar_client PROPERTIES OUTPUT_NAME "registrar_client")
set_target_properties(registrar_client PROPERTIES PREFIX "")
target_link_libraries(registrar_client ${CMAKE_DL_LIBS} pthread m)

