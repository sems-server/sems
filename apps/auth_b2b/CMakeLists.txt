set (auth_b2b_SRCS
AuthB2B.cpp
)

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMOD_NAME=\"auth_b2b\"")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMOD_NAME='\"auth_b2b\"'")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_library(auth_b2b SHARED ${auth_b2b_SRCS})
set_target_properties(auth_b2b PROPERTIES OUTPUT_NAME "auth_b2b")
set_target_properties(auth_b2b PROPERTIES PREFIX "")
target_link_libraries(auth_b2b ${CMAKE_DL_LIBS} pthread m)

