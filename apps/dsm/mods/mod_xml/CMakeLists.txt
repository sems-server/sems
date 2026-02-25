find_package(LibXml2 REQUIRED)

set(mod_xml_SRCS ModXml.cpp)

include_directories(${LIBXML2_INCLUDE_DIR})

set(sems_dsm_module_name mod_xml)
set(sems_dsm_module_libs ${LIBXML2_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)
