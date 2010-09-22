SRCS=$(wildcard *.txt)
WAVS=$(SRCS:.txt=.wav)

all: $(WAVS) 

%.wav : %.txt Makefile
##	flite -f $< -o $(basename $@)_16k.wav
	text2wave $< -o $(basename $@)_16k.wav
	sox $(basename $@)_16k.wav -r 8000 $@
	rm $(basename $@)_16k.wav
