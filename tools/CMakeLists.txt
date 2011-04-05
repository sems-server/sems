# executable

set (sems-logfile-callextract_SRCS
logfile-splitter.cpp
)

ADD_EXECUTABLE (sems-logfile-callextract ${sems-logfile-callextract_SRCS})
#TARGET_LINK_LIBRARIES(sems-stats ${CMAKE_DL_LIBS} stdc++)

INSTALL(TARGETS sems-logfile-callextract
        RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin
        )

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
