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

# needed for DSM sub-modules
INCLUDE_DIRECTORIES (.)

ADD_SUBDIRECTORY (lib)
ADD_SUBDIRECTORY (mods)

SET(sems_module_name dsm)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
