set (ilbc_SRCS
FrameClassify.c
LPCdecode.c
LPCencode.c
StateConstructW.c
StateSearchW.c
anaFilter.c
constants.c
createCB.c
doCPLC.c
enhancer.c
filter.c
gainquant.c
getCBvec.c
helpfun.c
hpInput.c
hpOutput.c
iCBConstruct.c
iCBSearch.c
iLBC_decode.c
iLBC_encode.c
lsf.c
packing.c
syntFilter.c
)

SET(CMAKE_VERBOSE_MAKEFILE ON)

ADD_LIBRARY(libilbc STATIC ${ilbc_SRCS})
SET_TARGET_PROPERTIES(libilbc PROPERTIES OUTPUT_NAME "ilbc")
TARGET_LINK_LIBRARIES(libilbc ${CMAKE_DL_LIBS} pthread m)

