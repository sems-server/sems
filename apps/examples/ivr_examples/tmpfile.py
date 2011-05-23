#
# using IvrAudioFile fpopen
#
from log import *
from ivr import *
import os

class IvrDialog(IvrDialogBase):

	def onSessionStart(self):

		info("starting tmpfile.py")
		f = open("/tmp/default_en.wav")
		audio = f.read()
		fp = os.tmpfile()
		fp.write(audio)
		fp.seek(0)
		self.wav = IvrAudioFile()
		self.wav.fpopen("tmp.wav", AUDIO_READ, fp)
		self.enqueue(self.wav, None)
		return

	def onEmptyQueue(self):

		if not self.queueIsEmpty():
			return
		self.bye()
		self.stopSession()
		return
