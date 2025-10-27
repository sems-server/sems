set(ilbc_SRCS ilbc.c)

set(sems_module_name ilbc)
if(ILBC_FOUND)
  set(sems_module_libs ${ILBC_LIBRARIES})
else(ILBC_FOUND)
  file(GLOB ilbc_SRCS ilbc.c "iLBC_rfc3951/*.c")
  include_directories(iLBC_rfc3951)
endif(ILBC_FOUND)

include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
