set (ilbc_SRCS
 ilbc.c
)

SET(sems_module_name ilbc)
IF(ILBC_FOUND)
SET(sems_module_libs ${ILBC_LIBRARIES})
ELSE(ILBC_FOUND)
FILE (GLOB ilbc_SRCS ilbc.c "iLBC_rfc3951/*.c")
INCLUDE_DIRECTORIES (iLBC_rfc3951)
ENDIF(ILBC_FOUND)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)

