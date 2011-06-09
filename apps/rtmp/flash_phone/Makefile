
FLEX_SDK=/opt/flex_sdk_4.1/bin

MXMLC=$(FLEX_SDK)/mxmlc
MXMLC_OPTS= \
	--strict=true \
	--as3=true \
	--target-player="10.1.0"

APP=sono
SOURCES= $(APP).mxml $(wildcard *.as)

all: $(APP).swf

clean:
	rm *.swf

%.swf: $(SOURCES) Makefile
	$(MXMLC) $(MXMLC_OPTS) $<