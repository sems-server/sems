set (dsm_SRCS
DSM.cpp
DSMChartReader.cpp
DSMCoreModule.cpp
DSMDialog.cpp
DSMElemContainer.cpp
DSMModule.cpp
DSMSession.cpp
DSMStateDiagramCollection.cpp
DSMStateEngine.cpp
)

INCLUDE_DIRECTORIES (.)

ADD_SUBDIRECTORY (mods)

ADD_LIBRARY(dsm SHARED ${dsm_SRCS})
SET_TARGET_PROPERTIES(dsm PROPERTIES OUTPUT_NAME "dsm")
TARGET_LINK_LIBRARIES(dsm ${CMAKE_DL_LIBS} pthread m)

