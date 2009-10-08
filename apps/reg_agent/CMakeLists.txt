set (reg_agent_SRCS
RegistrationAgent.cpp
)

ADD_LIBRARY(reg_agent SHARED ${reg_agent_SRCS})
TARGET_LINK_LIBRARIES(reg_agent ${CMAKE_DL_LIBS})

