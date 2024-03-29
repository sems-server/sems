FIND_PATH(CODEC2_INCLUDE_DIR codec2/codec2.h)
FIND_LIBRARY(CODEC2_LIBRARIES NAMES codec2)

IF(CODEC2_INCLUDE_DIR AND CODEC2_LIBRARIES)
	SET(CODEC2_FOUND YES)
ENDIF(CODEC2_INCLUDE_DIR AND CODEC2_LIBRARIES)

IF(CODEC2_FOUND)
	IF (NOT Codec2_FIND_QUIETLY)
		MESSAGE(STATUS "Found codec2 includes:	${CODEC2_INCLUDE_DIR}/codec2/codec2.h")
		MESSAGE(STATUS "Found codec2 library: ${CODEC2_LIBRARIES}")
	ENDIF (NOT Codec2_FIND_QUIETLY)
ELSE(CODEC2_FOUND)
	IF (Codec2_FIND_REQUIRED)
		MESSAGE(FATAL_ERROR "Could NOT find codec2 development files")
	ENDIF (Codec2_FIND_REQUIRED)
ENDIF(CODEC2_FOUND)
