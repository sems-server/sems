import base64,time

from log import *
from ivr import *

from imap_mailbox.imap4ext import *


class IvrDialog(IvrDialogBase):

	announcement = None
	beep         = None
	voice_msg    = None
	mailbox      = None

	def onSessionStart(self):

		debug("config: %s" % repr(config))
		self.mailbox = IMAP4_Mailbox(getAppParam("Mailbox-URL"));
		debug("***** Mailbox Url: ******\n" + str(self.mailbox.url))

		self.announcement = IvrAudioFile()
		self.announcement.open(config['annoucement_file'],AUDIO_READ)

		self.beep = IvrAudioFile()
		self.beep.open(config['beep_file'],AUDIO_READ)

		self.voice_msg = IvrAudioFile()
		self.voice_msg.open("tmp.wav",AUDIO_WRITE,True)
		self.voice_msg.setRecordTime(30*1000) # 30s

		self.enqueue(self.announcement,None)
		self.enqueue(self.beep,None)
		self.enqueue(None,self.voice_msg)
		
		
	def onBye(self):
		
		self.stopSession()
		self.saveMsg()


	def onEmptyQueue(self):

		

		self.bye()
		self.stopSession()
		self.saveMsg()


	def onDtmf(self,key,duration):
		
		pass


	def saveMsg(self):

		if not self.voice_msg.getDataSize():
			return

		outfile = self.voice_msg.exportRaw()
		outfile.seek(0)
		msg = outfile.read()
		enc_msg = base64.encodestring(msg)
		msg = "From: voicemail@tweety\n" +\
		      "MIME-Version: 1.0\n" +\
		      "Content-Type: audio/x-wav\n" +\
		      "Content-Disposition: inline;\n filename=\"msg.wav\"\n" +\
		      "Content-Transfer-Encoding: base64\n\n" +\
		      enc_msg

		#debug("msg = <%s>",msg);
		
		self.mailbox.uploadMsg(msg)
