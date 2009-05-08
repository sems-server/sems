set (callback_SRCS
CallBack.cpp
)

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMOD_NAME=\"callback\"")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMOD_NAME='\"callback\"'")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_library(callback SHARED ${callback_SRCS})
set_target_properties(callback PROPERTIES OUTPUT_NAME "callback")
set_target_properties(callback PROPERTIES PREFIX "")
target_link_libraries(callback ${CMAKE_DL_LIBS} pthread m)

