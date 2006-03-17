from log import *
from ivr import *
import xmlrpc

XMLRPC_SERVER   = 'localhost'
XMLRPC_PORT	= 23456
XMLRPC_DIR      = '/blah'
XMLRPC_LOGLEVEL	= 3		# this is the default log level

collect    = 0
connect    = 1
connected  = 2

HINT_TIMER   = 1
HINT_TIMEOUT = 10 # seconds until hint msg is played

CONF_TIMER   = 2

def loopedTTS(str):
	af = IvrAudioFile.tts(str)
	af.loop = True
	return af

class IvrDialog(IvrDialogBase):
	welcome_msg   = None
	auth_ok_msg   = None
	auth_fail_msg = None
	hint_msg      = None

	state = collect
	keys = ''

	conf_to = ''
	conf_uri = ''
	conf_duration = 0
	conf_participants = 0
		
	def sessionInfo(self):
		print "IVR Session info:"
		print " user:        ", self.dialog.user
		print " domain:      ", self.dialog.domain
		print " sip_ip:      ", self.dialog.sip_ip
		print " sip_port:    ", self.dialog.sip_port
		print " local_uri:   ", self.dialog.local_uri
		print " remote_uri:  ", self.dialog.remote_uri
		print " contact_uri: ", self.dialog.contact_uri
		print " callid:      ", self.dialog.callid
		print " remote_tag:  ", self.dialog.remote_tag
		print " local_tag:   ", self.dialog.local_tag
		print " remote_party:", self.dialog.remote_party
		print " local_party: ", self.dialog.local_party
		print " route:       ", self.dialog.route
		print " next_hop:     ", self.dialog.next_hop
		print " cseq:        ", self.dialog.cseq

	def onSessionStart(self):
		self.sessionInfo()
		self.setNoRelayonly()
		self.welcome_msg = IvrAudioFile.tts("Welcome to the conferencing server. "+ \
						"Please enter your PIN number, followed by the star key.")
		self.auth_ok_msg = IvrAudioFile.tts("Your PIN number is correct, you will be connected now.")
#		self.auth_fail_msg = IvrAudioFile.tts("Sorry, you have entered an invalid PIN number. " + \
#							"Please try again.")
		self.hint_msg = IvrAudioFile.tts("Please enter your PIN number, followed by the star key.")

		self.enqueue(self.welcome_msg,None)
		xmlrpc.setLogLevel(XMLRPC_LOGLEVEL)

	def onBye(self):		
		self.stopSession()

	def onDtmf(self,key,duration):
		if self.state == collect:
			if key == 10:
				c = xmlrpc.client(XMLRPC_SERVER, XMLRPC_PORT, XMLRPC_DIR)
				erg = c.execute('AuthorizeConference', [self.dialog.remote_uri, \
									self.dialog.local_party, self.keys ])
				print 'result of auth call: ' 
				print erg
				if erg[0] == 'OK':
					self.flush()
					self.enqueue(self.auth_ok_msg, None)				
					self.conf_to = erg[1]
					self.conf_uri = erg[2]
					self.conf_duration = erg[3]
					self.conf_participants = erg[4]
					self.state = connect
					self.removeTimer(HINT_TIMER)
				else:
					self.auth_fail_msg = IvrAudioFile()
					self.auth_fail_msg.open(erg[1], ivr.AUDIO_READ)
					self.flush()
					self.enqueue(self.auth_fail_msg, None)
					self.keys = ''
					self.setTimer(HINT_TIMER, HINT_TIMEOUT)
			else: 
				self.keys += str(key)
				print "added key, PIN = " + self.keys
				self.setTimer(HINT_TIMER, HINT_TIMEOUT)

	def onEmptyQueue(self):
		if self.state == connect:
			print "connecting to " + self.conf_to + "uri: " + self.conf_uri
			self.mute()
			self.setRelayonly()
			self.connectCallee(self.conf_to, self.conf_uri)
			self.state = connected
			self.setTimer(CONF_TIMER, self.conf_duration)

		elif self.state == collect:
			self.setTimer(HINT_TIMER, HINT_TIMEOUT)

	def onTimer(self, id):
		if id == HINT_TIMER and self.state == collect:
			self.enqueue(self.hint_msg, None)
		elif id == CONF_TIMER and self.state == connected:
			self.terminateOtherLeg()
			self.bye()
			self.stopSession()
	
