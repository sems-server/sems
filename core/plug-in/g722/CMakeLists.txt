set(g722_SRCS g722.c)

set(sems_module_name g722)
set(sems_module_libs ${SPANDSP_LIBRARIES})
set(g722_doc_files Readme.g722codec)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)
