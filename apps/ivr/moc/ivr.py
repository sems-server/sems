
config = dict()

AUDIO_READ  = 0
AUDIO_WRITE = 1

def getHeader(str1,str2):
	pass

class IvrDialogBase:

	def enqueue(self,a1,a2):
		pass

	def flush(self):
		pass

	def bye(self):
		pass
	
	def stopSession(self):
		pass

class IvrAudioFile:

	def open(self,filename,mode):
		pass
