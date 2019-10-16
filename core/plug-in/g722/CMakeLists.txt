set (g722_SRCS
g722.c
)

SET(sems_module_name g722)
SET(sems_module_libs ${SPANDSP_LIBRARIES})
SET(g722_doc_files Readme.g722codec)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)
