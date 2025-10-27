set(g729_SRCS g729.c)

set(sems_module_name g729)
set(sems_module_libs ${BCG729_LIBRARIES})
set(g729_doc_files Readme.g729.txt)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)
