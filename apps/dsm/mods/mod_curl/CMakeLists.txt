find_package(CURL REQUIRED)

set(mod_curl_SRCS ModCurl.cpp)

include_directories(${CURL_INCLUDE_DIRS})

set(sems_dsm_module_name mod_curl)
set(sems_dsm_module_libs ${CURL_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)
