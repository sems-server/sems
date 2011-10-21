from log import *
from ivr import *
from twyt import *
from twyt import twitter, data
from urllib import *
import random


base_url = config['base_url']
rec_path = config['rec_path']
welcome_msg = config['welcome_msg']
twit_err_msg = config['twit_err_msg']
twit_account_msg = config['twit_account_msg']
twit_ok_msg = config['twit_ok_msg']
twit_posting_msg = config['twit_posting_msg']

class IvrDialog(IvrDialogBase) :

	def onSessionStart( self ) :
		self.t_msg = 'mirps! mirps! from iptel: '
		account_found = False
		if self.dialog.user.find(';')>=0:
			parts = self.dialog.user.split(';',3)
			if len(parts) >= 3:
				account_found = True
				self.t_account = parts[1]
				self.t_pwd = parts[2]
				if len(parts) == 4:
					self.t_msg = unquote(parts[3])
		
		if not account_found:		
			self.t_account = getAppParam("u")
			self.t_pwd = getAppParam("p")
			if (len(getAppParam("m"))) and getAppParam("m") != '<null>':
				self.t_msg = unquote(getAppParam("m"))
			if (self.t_account != '<null>' and self.t_account != '') and \
				self.t_pwd != '<null>' and self.t_pwd != '':
				account_found = True	
			
		if not account_found:
			self.state = 'bye'
			self.welcome_msg = IvrAudioFile()
			self.welcome_msg.open(twit_account_msg, AUDIO_READ)
			self.enqueue(self.welcome_msg, None)
			return
		
		self.state = 'recording'
		self.welcome_msg = IvrAudioFile()
		self.welcome_msg.open(welcome_msg, AUDIO_READ)
		self.enqueue(self.welcome_msg, None)
		
		self.audio_msg = IvrAudioFile()
		self.twit_file = 'twit_' + ''.join(random.sample('abcdefghijklm', 8)) + '.wav'
		self.audio_msg.open(rec_path + self.twit_file, AUDIO_WRITE)
		self.enqueue(None,self.audio_msg)

		self.setTimer(1, 180)
		
	def onTimer(self, id):
		onDtmf(self, 0, 0)
		pass
	
	def onDtmf(self, key_no, key_length):
		self.flush()
		self.state = 'recorded'
		
	def onBye(self):
		self.stopSession()
	
	def onEmptyQueue(self):
		log(1, 'eq in '+self.state)
		if self.state == 'bye':
			self.bye()
			self.stopSession()
		elif self.state == 'recorded':
			self.state = 'bye'
			self.posting_msg = IvrAudioFile()
			self.posting_msg.open(twit_posting_msg, AUDIO_READ)
			self.enqueue(self.posting_msg, None)
			
			longurl=base_url + self.twit_file
			short_url_handle = urlopen('http://is.gd/api.php?longurl=%s' % longurl)
			msg = twit_ok_msg
			try:
				t = twitter.Twitter()
				t.set_auth(self.t_account, self.t_pwd)
				return_val = t.status_update(self.t_msg+' @'+short_url_handle.read())
			except:
				msg = twit_err_msg
			self.err_msg = IvrAudioFile()
			self.err_msg.open(msg, AUDIO_READ)
			self.enqueue(self.err_msg, None)


