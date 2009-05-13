set (reg_agent_SRCS
RegistrationAgent.cpp
)

ADD_LIBRARY(reg_agent SHARED ${reg_agent_SRCS})
SET_TARGET_PROPERTIES(reg_agent PROPERTIES OUTPUT_NAME "reg_agent")
SET_TARGET_PROPERTIES(reg_agent PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(reg_agent ${CMAKE_DL_LIBS} pthread m)

