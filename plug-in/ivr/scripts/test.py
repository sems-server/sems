from log import *
from ivr import *


wav_path = "/home/rco/src/iptelsvn/scratch/rco/sems/new_audio/wav/"


class IvrDialog(IvrDialogBase):

	main_menu   = None
	listen_menu = None
	config_menu = None
	
	menu = 0

	def onSessionStart(self):

		self.main_menu = IvrAudioFile()
		
		self.main_menu.tts("Welcome to your personal voicebox." + \
				   "Press 1 to listen to your messages." + \
				   "Press 2 to configure your personal voicebox.")
		
		self.enqueue(self.main_menu,None)


	def onBye(self):
		
		self.stopSession()

	
	def onEmptyQueue(self):
		
		if self.menu == 0:
			self.main_menu.tts("Welcome to your personal voicebox." + \
					   "Press 1 to listen to your messages." + \
					   "Press 2 to configure your personal voicebox.")
			self.enqueue(self.main_menu,None)

		elif self.menu == 1:
			if self.listen_menu == None:
				self.listen_menu = IvrAudioFile()
			self.listen_menu.tts("Listen to your messages and then " + \
					     "press 0 to return to the main menu.")
			self.enqueue(self.listen_menu,None)

		elif self.menu == 2:
			if self.config_menu == None:
				self.config_menu = IvrAudioFile()
			self.config_menu.tts("Configure you voicebox and then " + \
					     "press 0 to return to the main menu.")
			self.enqueue(self.config_menu,None)
		else:
			self.bye()
			self.stopSession()
	

	def onDtmf(self,key,duration):

		if self.menu == 0:
			if key == 1:
				self.menu = 1
				self.flush()

			if key == 2:
				self.menu = 2
				self.flush()
		else:
			if key == 0:
				self.menu = 0
				self.flush()
