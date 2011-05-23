from log import *
from ivr import *
import xmlrpclib


XMLRPC_PROTOCOL = 'http'         # could e.g. be https
XMLRPC_SERVER   = 'localhost'
XMLRPC_PORT	= 23456
XMLRPC_DIR      = '/conf_auth'

HINT_TIMEOUT    = 10              # seconds until hint msg is played

server_path = XMLRPC_PROTOCOL + '://' +  XMLRPC_SERVER + \
		':' +  str(XMLRPC_PORT) +   XMLRPC_DIR

collect    = 0
connect    = 1
connected  = 2

HINT_TIMER   = 1
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

	def onBye(self):		
		self.stopSession()

	def onDtmf(self,key,duration):
		if self.state == collect:
			self.flush()
			if key == 10:
	
				c = xmlrpclib.ServerProxy(server_path )
				erg = c.AuthorizeConference(self.dialog.remote_uri, 
									self.dialog.local_party, self.keys)

				debug('result of authentication: '+ str(erg))
				if erg[0] == 'OK':
					self.flush()
					self.enqueue(self.auth_ok_msg, None)				
					self.conf_to = erg[1]
					self.conf_uri = erg[2]
					self.conf_duration = erg[3]
					self.state = connect
					self.removeTimer(HINT_TIMER)
				else:
					self.keys = ''
					self.auth_fail_msg = IvrAudioFile()
					self.auth_fail_msg.open(erg[1], ivr.AUDIO_READ)
					#self.flush()
					self.enqueue(self.auth_fail_msg, None)
					
					self.setTimer(HINT_TIMER,  HINT_TIMEOUT)
			else: 
				self.keys += str(key)
				debug("added key, PIN = " + self.keys)
				self.setTimer(HINT_TIMER,  HINT_TIMEOUT)

	def onEmptyQueue(self):
		if self.state == connect:
			debug("connecting to " + self.conf_to + "uri: " + self.conf_uri)
			self.disconnectMedia()
			self.mute()
			self.connectCallee(self.conf_to, self.conf_uri)
			self.state = connected
			if (self.conf_duration > 0):
				self.setTimer(CONF_TIMER, self.conf_duration)

		elif self.state == collect:
			self.setTimer(HINT_TIMER,  HINT_TIMEOUT)

 	def onTimer(self, id):
		if id == HINT_TIMER and self.state == collect:
			self.enqueue(self.hint_msg, None)
		elif id == CONF_TIMER and self.state == connected:
			debug("conference timer timeout. disconnecting " + self.conf_to + " uri: " + self.conf_uri)
			self.terminateOtherLeg()
			self.bye()
			self.stopSession()
	
