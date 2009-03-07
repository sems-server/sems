
all: participant.py conftable.py callbox.py account.py

install:
	cp *.py tablet/conf

%.py: %.ui
	pyuic4 $< -o $@
