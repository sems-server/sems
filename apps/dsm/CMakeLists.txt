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

add_library(dsm SHARED ${dsm_SRCS})
set_target_properties(dsm PROPERTIES OUTPUT_NAME "dsm")
set_target_properties(dsm PROPERTIES PREFIX "")
target_link_libraries(dsm ${CMAKE_DL_LIBS} pthread m)

