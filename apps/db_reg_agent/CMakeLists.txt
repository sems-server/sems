set (db_reg_agent_SRCS
DBRegAgent.cpp
RegistrationTimer.cpp
)

INCLUDE_DIRECTORIES(/usr/include/mysql)
INCLUDE_DIRECTORIES(${MYSQLPP_INCLUDE_DIR}/mysql++)
ADD_DEFINITIONS(-DHAS_OFFER_ANSWER)
SET(sems_module_libs ${sems_module_libs} mysqlpp)

SET(sems_module_name db_reg_agent)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
