set (gsm_SRCS
 gsm.c
)

add_library(gsm SHARED ${gsm_SRCS})
set_target_properties(gsm PROPERTIES OUTPUT_NAME "gsm")
set_target_properties(gsm PROPERTIES PREFIX "")
target_link_libraries(gsm ${CMAKE_DL_LIBS} pthread ${GSM_LIBRARIES} m)

