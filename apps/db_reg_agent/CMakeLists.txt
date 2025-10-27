set(db_reg_agent_SRCS DBRegAgent.cpp RegistrationTimer.cpp)

include_directories(/usr/include/mysql)
include_directories(${MYSQLPP_INCLUDE_DIR}/mysql++)
add_definitions(-DHAS_OFFER_ANSWER)
set(sems_module_libs ${sems_module_libs} ${MYSQLPP_LIBRARIES})

set(sems_module_name db_reg_agent)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
