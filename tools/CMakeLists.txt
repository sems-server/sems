# executable

set (sems-logfile-callextract_SRCS
logfile-splitter.cpp
)

ADD_EXECUTABLE (sems-logfile-callextract ${sems-logfile-callextract_SRCS})

INSTALL(TARGETS sems-logfile-callextract
        RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin
        )

INSTALL(PROGRAMS sems-rtp-mux-get-max-frame-age-ms
		 sems-rtp-mux-get-mtu-threshold
		 sems-rtp-mux-set-max-frame-age-ms
		 sems-rtp-mux-set-mtu-threshold
        DESTINATION ${SEMS_EXEC_PREFIX}/sbin
        )

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
