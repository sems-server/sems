#
# Simple implementation of (a part) of RFC4240 
# announcement service.
#
# supported parameters:
#  play, repeat, duration, delay


from log import *
from ivr import *

from urlparse import urlparse, urlsplit
from urllib import urlretrieve
from os import unlink

TIMEOUT_TIMER_ID = 1
DELAY_TIMER_ID   = 2

class IvrDialog(IvrDialogBase):

	announcement=None
	filename = ""
	repeat="1"
	delay=0
	duration=-1
	play=""
	delete_onbye = False	
	repeat_left = 0

	def onSessionStart(self,hdrs):

		debug("configuration: %s" % repr(config))
		debug("local_uri = " + self.dialog.local_uri);
		# we use urlsplit as urlparse only returns the
		# parameters of the last path
		params = urlsplit(self.dialog.local_uri)[2].split(";")
		debug("parameters are " + str(params))
		for param in params[0:len(params)]:
			if (param.startswith("play=")):
				self.play=param[5:len(param)]
			elif (param.startswith("repeat=")):
				self.repeat=param[7:len(param)]
			elif (param.startswith("delay=")):
				self.delay=int(param[6:len(param)])
			elif (param.startswith("duration=")):
				self.duration=int(param[9:len(param)])
		
		resource = urlparse(self.play)
		if (resource[0] == "http"):
			self.delete_onbye = True
		self.filename = urlretrieve(self.play)[0]

		debug("play: "+self.play+" repeat: "+self.repeat+" delay:"+
			str(self.delay)+" duration: "+str(self.duration))
		self.announcement = IvrAudioFile()
		self.announcement.open(self.filename,AUDIO_READ)
		if (self.repeat!="forever"):
			self.repeat_left=int(self.repeat)-1
		else:
			self.repeat_left=500 # maximum

		if (int(self.duration) > 0):
			self.setTimer(TIMEOUT_TIMER_ID, self.duration/1000)
			
		self.enqueue(self.announcement, None)
		
		
	def onBye(self):
		self.stopSession()
		self.cleanup()

	def onEmptyQueue(self):
 		if (self.repeat_left>0):
			if (int(self.delay) > 0):
				self.setTimer(DELAY_TIMER_ID, int(self.delay)/1000)
			else:
				self.repeat_left-=1
				self.announcement.rewind()
				self.enqueue(self.announcement, None)
		else:
			self.bye()
			self.stopSession()
			self.cleanup()

	def onDtmf(self,key,duration):
		pass

	def onTimer(self, timer_id):
		if (timer_id == TIMEOUT_TIMER_ID):
			self.bye()
			self.stopSession()
			self.cleanup()
		elif (timer_id == DELAY_TIMER_ID):
				self.repeat_left-=1
				self.announcement.rewind()
				self.enqueue(self.announcement, None)

	def cleanup(self):
		if (self.delete_onbye):
			unlink(self.filename)
			debug("cleanup..." + self.filename + " deleted.")
		self.removeTimers()


