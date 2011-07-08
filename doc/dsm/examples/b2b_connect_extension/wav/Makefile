SRCS=$(wildcard *.txt)
WAVS=$(SRCS:.txt=.wav)

all: 

	@echo "use 'make tts' to generate prompts."
tts: $(WAVS) 

%.wav : %.txt Makefile
	flite -f $< -o $(basename $@)_16k.wav
	sox $(basename $@)_16k.wav -r 8000 $@
