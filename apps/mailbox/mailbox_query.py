import os,weakref

from log import *
from ivr import *

from imap_mailbox.imap4ext import *


class List(list):
	pass


class VoiceMsg:

	def __init__(self,uid,mb):

		self._uid   = uid
		self._mb    = mb
		self._audio = None


	def _load(self):

		self._fp    = self._mb.downloadWAV(self._uid)
		self._audio = IvrAudioFile()
		self._audio.fpopen("tmp.wav",AUDIO_READ,self._fp)


	def audio(self):

		if self._audio == None:
			self._load()
			
		return self._audio


	def save(self):

		self._mb.saveMsg(self._uid)


	def delete(self):

		self._mb.deleteMsg(self._uid)




class IvrDialog(IvrDialogBase):

	def __init__(self):

		self.voice_lib = None

		self.cur_list   = None
		self.msg_list   = None
		self.new_msgs   = List()
		self.saved_msgs = List()

		self.cur_msg = 0
		self.exit    = False
		
		self.key_enabled = False


	def loadVoiceLib(self):
		
		self.voice_lib = dict()

		# Parts for the welcome text
		self.voice_lib['you_have'] = None
		self.voice_lib['new_msg'] = None
		self.voice_lib['saved_msg'] = None
		self.voice_lib['no_msg'] = None
		self.voice_lib['and'] = None

		# Menu played after each message
		self.voice_lib['msg_menu'] = None

		# Status acknowledgement
		self.voice_lib['msg_deleted'] = None
		self.voice_lib['msg_saved'] = None
		self.voice_lib['first_msg'] = None
		self.voice_lib['next_msg'] = None

		# End of conversation
		self.voice_lib['bye'] = None

		# Pre-Load the WAVs
		for k in self.voice_lib.keys():
			wav = IvrAudioFile()
			wav.open(config['wav_dir'] + '/' + k + '.wav',AUDIO_READ)
			self.voice_lib[k] = wav

		# Menu will be looped until a key is pressed
		self.voice_lib['msg_menu'].loop = True


	def loadMsgList(self,criterion):

		msg_list = List()
		name_list = self.mailbox.getWavMsgList(criterion)
		for n in name_list:
			msg_list.append(VoiceMsg(n,self.mailbox))
			
		return msg_list


	def onSessionStart(self):

		self.__init__()
		self.loadVoiceLib()

		self.mailbox = IMAP4_Mailbox(getAppParam("Mailbox-URL"));
		debug("***** Mailbox Url: ******\n" + str(self.mailbox.url))

		self.new_msgs = self.loadMsgList('UNSEEN')
		self.saved_msgs = self.loadMsgList('SEEN')

		if (len(self.new_msgs) == 0) and \
		       (len(self.saved_msgs) == 0):
			
			self.enqueue(self.voice_lib['no_msg'],None)
			self.enqueue(self.voice_lib['bye'],None)
			self.exit = True
			return
		
		self.enqueue(self.voice_lib['you_have'],None)

		if len(self.new_msgs) > 0:
			
			self.enqueue(self.voice_lib['new_msg'],None)
			self.msg_list = weakref.proxy(self.new_msgs)
			self.cur_list = weakref.ref(self.new_msgs)
			
			if len(self.saved_msgs) > 0:
				self.enqueue(self.voice_lib['and'],None)
				self.enqueue(self.voice_lib['saved_msg'],None)
		else:
			self.enqueue(self.voice_lib['saved_msg'],None)
			self.msg_list = weakref.proxy(self.saved_msgs)
			self.cur_list = weakref.ref(self.saved_msgs)

		self.enqueueCurMsg()
		

	def onBye(self):
		
		self.stopSession()

	
	def onEmptyQueue(self):

		if self.exit:
			if self.queueIsEmpty():
				self.bye()
				self.stopSession()


	def onDtmf(self,key,duration):

		if not self.key_enabled:
			return

		debug("onDtmf(%i,%i)" % (key,duration))

		if key == 1:
			self.flush()
			

		elif key == 2:
			self.key_enabled = False
			self.flush()
			self.msg_list[self.cur_msg].save()
			self.enqueue(self.voice_lib['msg_saved'],None)
			self.cur_msg += 1
			
		elif key == 3:
			self.key_enabled = False
			self.flush()
			self.msg_list[self.cur_msg].delete()
			self.enqueue(self.voice_lib['msg_deleted'],None)
			self.cur_msg += 1
		else:
			return
			
		if not self.enqueueCurMsg():

			debug("self.cur_list() is self.new_msgs = %s" % repr(self.cur_list() is self.new_msgs))
			debug("len(self.saved_msgs) = %i" % len(self.saved_msgs))
			
			if (self.cur_list() is self.new_msgs) and \
			   len(self.saved_msgs) > 0:
				
				self.cur_msg = 0
				
				self.msg_list = weakref.proxy(self.saved_msgs)
				self.cur_list = weakref.ref(self.saved_msgs)
				
				self.enqueue(self.voice_lib['saved_msg'],None)
				self.enqueueCurMsg()
			else:
				self.enqueue(self.voice_lib['bye'],None)
				self.exit = True


	def enqueueCurMsg(self):

		if self.cur_msg >= len(self.msg_list):
			return False

		if self.cur_msg > 0:
			self.enqueue(self.voice_lib['next_msg'],None)
		else:
			self.enqueue(self.voice_lib['first_msg'],None)
			
		self.enqueue(self.msg_list[self.cur_msg].audio(),None)
		self.enqueue(self.voice_lib['msg_menu'],None)
		
		self.key_enabled = True

		return True
