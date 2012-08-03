set (ilbc_SRCS
 ilbc.c
)

SET(sems_module_name ilbc)
IF(ILBC_FOUND)
SET(sems_module_libs ilbc)
ELSE(ILBC_FOUND)
ADD_SUBDIRECTORY (iLBC_rfc3951)
INCLUDE_DIRECTORIES (iLBC_rfc3951)
SET(sems_module_libs libilbc)
ENDIF(ILBC_FOUND)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)

