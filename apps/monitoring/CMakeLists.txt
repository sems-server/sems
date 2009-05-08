set (monitoring_SRCS
Monitoring.cpp
)

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMOD_NAME=\"monitoring\"")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMOD_NAME='\"monitoring\"'")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_library(monitoring SHARED ${monitoring_SRCS})
set_target_properties(monitoring PROPERTIES OUTPUT_NAME "monitoring")
set_target_properties(monitoring PROPERTIES PREFIX "")
target_link_libraries(monitoring ${CMAKE_DL_LIBS} pthread m)

