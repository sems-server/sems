#
# testing mp3: play play.mp3, then record record.mp3
#

from log import *
from ivr import *
class IvrDialog(IvrDialogBase):

	def onSessionStart(self):
		self.voice_msg = IvrAudioFile()
		self.voice_msg.open("play.mp3", AUDIO_READ, False)
		self.enqueue(self.voice_msg, None)
		self.voice_msg.open("record.mp3", AUDIO_WRITE, False)
		self.enqueue(None, self.voice_msg)

