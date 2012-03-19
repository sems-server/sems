#
# The Sono flash app requires at least Flash player 10.3 (because of the AEC)
#
# In order to compile properly, following is required:
#   - Flex SDK 4.5.1 (or higher)
#   - playerglobal.swc for flash 10.3 (if not included)
#     (download from http://www.adobe.com/support/flashplayer/downloads.html)
#     (and install in FLEX_SDK/framworks/libs/player/10.3/)
#

# Change this to your Flex SDK directory
FLEX_SDK=/opt/flex_sdk_4.5

MXMLC=$(FLEX_SDK)/bin/mxmlc
MXMLC_OPTS= \
	--strict=true \
	--as3=true \
	-target-player 11.0 \
	-swf-version=13

APP=sono
SOURCES= $(APP).mxml $(wildcard *.as)

all: $(APP).swf

clean:
	rm *.swf

%.swf: $(SOURCES) Makefile
	$(MXMLC) $(MXMLC_OPTS) $<
