set (ilbc_SRCS
 ilbc.c
)

ADD_SUBDIRECTORY (iLBC_rfc3951)
INCLUDE_DIRECTORIES (iLBC_rfc3951)

SET(sems_module_name ilbc)
SET(sems_module_libs libilbc)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)

