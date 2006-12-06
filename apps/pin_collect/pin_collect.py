from log import *
from ivr import *

collect    = 0
connect    = 1
connected  = 2

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
				self.redirect("sip:" + self.dialog.user + \
					      self.keys + "@" + \
					      self.dialog.domain)
				self.stopSession()

	def onEmptyQueue(self):
		if self.state == collect:
			self.setTimer(HINT_TIMER, HINT_TIMEOUT)

	def onTimer(self, id):
		if id == HINT_TIMER and self.state == collect:
			self.enqueue(self.pin_msg, None)
