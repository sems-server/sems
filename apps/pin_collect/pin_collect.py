from log import *
from ivr import *

collect         = 0
connect         = 1
connect_failed  = 2


HINT_TIMER   = 1
HINT_TIMEOUT = 10 # seconds until hint msg is played

CONF_TIMER   = 2

# def loopedTTS(str):
# 	af = IvrAudioFile.tts(str)
# 	af.loop = True
# 	return af

class IvrDialog(IvrDialogBase):

	welcome_msg   = None
	auth_ok_msg   = None
	auth_fail_msg = None
	pin_msg       = None

	transfer_cseq    = None
	dlg_remote_uri   = None	

	state = collect
	keys = ''

	conf_to = ''
	conf_uri = ''
	conf_duration = 0
	conf_participants = 0
		
	def sessionInfo(self):
		debug("IVR Session info:")
		debug(" user:        " + self.dialog.user)
		debug(" domain:      " + self.dialog.domain)
		debug(" sip_ip:      " + self.dialog.sip_ip)
		debug(" sip_port:    " + self.dialog.sip_port)
		debug(" local_uri:   " + self.dialog.local_uri)
		debug(" remote_uri:  " + self.dialog.remote_uri)
		debug(" contact_uri: " + self.dialog.contact_uri)
		debug(" callid:      " + self.dialog.callid)
		debug(" remote_tag:  " + self.dialog.remote_tag)
		debug(" local_tag:   " + self.dialog.local_tag)
		debug(" remote_party:" + self.dialog.remote_party)
		debug(" local_party: " + self.dialog.local_party)
		debug(" route:       " + self.dialog.route)
		debug(" next_hop:    " + self.dialog.next_hop)
		debug(" cseq:        " + str(self.dialog.cseq))

	def onSessionStart(self,hdrs):
		self.sessionInfo()
		self.setNoRelayonly()
		
		self.welcome_msg = IvrAudioFile()
		self.welcome_msg.open(config['welcome_msg'],AUDIO_READ)
		
 		self.pin_msg = IvrAudioFile()
		self.pin_msg.open(config['pin_msg'],AUDIO_READ)

		self.enqueue(self.welcome_msg,None)
		self.enqueue(self.pin_msg,None)

	def onBye(self):
		self.stopSession()

	def onDtmf(self,key,duration):
		if self.state == collect:
			if key < 10:
				self.keys += str(key)
				debug("added key, PIN = " + self.keys)
				self.setTimer(HINT_TIMER, HINT_TIMEOUT)
			elif key == 10:
				self.state = connect
				self.removeTimer(HINT_TIMER)
				self.dlg_remote_uri	= self.dialog.remote_uri
				debug("saved remote_uri "+ self.dlg_remote_uri)
				self.transfer_cseq = self.dialog.cseq
				self.redirect("sip:" + self.dialog.user + "@" + \
					      self.dialog.domain)

	def onEmptyQueue(self):
		if self.state == collect:
			self.setTimer(HINT_TIMER, HINT_TIMEOUT)
		elif self.state == connect_failed:
			debug("transfer failed. stopping session.")
			debug("restoring  remote_uri to " + self.dlg_remote_uri)
			self.dialog.remote_uri = self.dlg_remote_uri
			self.bye()
			self.stopSession()

	def onTimer(self, id):
		if id == HINT_TIMER and self.state == collect:
			self.enqueue(self.pin_msg, None)

	def onSipReply(self, reply):
		if reply.cseq == self.transfer_cseq:
			# restore remote uri of dialog
			if reply.code >= 200 and reply.code < 300:
				debug("received positive reply to transfer request. dropping session.")
				self.dropSession()
			elif reply.code >= 300:
				debug("transfer failed. notifying user")
				self.state = connect_failed		
				self.fail_msg = IvrAudioFile()
				self.fail_msg.open(config['fail_msg'],AUDIO_READ)
				self.enqueue(self.fail_msg,None)
