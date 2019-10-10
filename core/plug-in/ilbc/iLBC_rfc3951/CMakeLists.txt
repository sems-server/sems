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

ADD_LIBRARY(libilbc STATIC ${ilbc_SRCS})
SET_PROPERTY(TARGET libilbc PROPERTY POSITION_INDEPENDENT_CODE ON)
