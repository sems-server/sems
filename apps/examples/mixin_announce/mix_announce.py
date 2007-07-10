
from log import *
from ivr import *

class IvrDialog(IvrDialogBase):
	mixin = None
	announcement = None
	beep = None

	def onSessionStart(self, hdrs):
		debug("onSessionStart of ivr announcement app")
		self.announcement = IvrAudioFile()
		self.announcement.open(config['announcement'], ivr.AUDIO_READ)
		#self.enqueue(self.announcement, None)
		
		self.beep = IvrAudioFile()
	        self.beep.open(config['beep'], ivr.AUDIO_READ)
		
		self.mixin = IvrAudioMixIn()
		self.mixin.init(self.announcement, self.beep, 3, 0.6, True)
                self.enqueue(self.mixin, None)

	def onEmptyQueue(self):
		self.bye()
		self.stopSession()
