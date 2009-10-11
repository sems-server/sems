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

ADD_SUBDIRECTORY (lib)
ADD_SUBDIRECTORY (mods)

SET(sems_module_name dsm)
SET(sems_config_files dsm_in_prompts dsm_out_prompts)
SET(doc_files
	./doc/Readme.dsm.txt
	./doc/dsm_syntax.txt
	./doc/dsm_todo.txt
)
SET(doc_dirs ./doc/examples)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)
