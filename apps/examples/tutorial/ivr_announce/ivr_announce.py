
from log import *
from ivr import *

class IvrDialog(IvrDialogBase):
	announcement = None

	def onSessionStart(self, hdrs):
		debug("onSessionStart of ivr announcement app")
		self.announcement = IvrAudioFile()
		self.announcement.open(config['announcement'], ivr.AUDIO_READ)
		self.enqueue(self.announcement, None)

	def onEmptyQueue(self):
		self.bye()
		self.stopSession()
