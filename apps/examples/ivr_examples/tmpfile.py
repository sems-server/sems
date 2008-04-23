#
# using IvrAudioFile fpopen
#
from log import *
from ivr import *
import os
class IvrDialog(IvrDialogBase):

	def onSessionStart(self, hdrs):
		f = open("wav/default_en.wav")
		audio = f.read()
 		debug("Found audio file of length " + str(len(audio)))
		fp = os.tmpfile()
		fp.write(audio)
		fp.seek(0)
		wav = IvrAudioFile()
		wav.fpopen("tmp.wav", AUDIO_READ, fp)
		self.enqueue(wav, None)
