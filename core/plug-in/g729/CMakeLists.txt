set (g729_SRCS
g729.c
)

SET(sems_module_name g729)
SET(sems_module_libs bcg729)
SET(g729_doc_files Readme.g729.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)
