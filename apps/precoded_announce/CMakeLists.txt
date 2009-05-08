set (precoded_announce_SRCS
PrecodedAnnounce.cpp
)

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMOD_NAME=\"precoded_announce\"")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMOD_NAME='\"precoded_announce\"'")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_library(precoded_announce SHARED ${precoded_announce_SRCS})
set_target_properties(precoded_announce PROPERTIES OUTPUT_NAME "precoded_announce")
set_target_properties(precoded_announce PROPERTIES PREFIX "")
target_link_libraries(precoded_announce ${CMAKE_DL_LIBS} pthread m)

