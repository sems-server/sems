
import re

class InvalidMailboxURL(Exception):	
	def __init__(self,value):
		self.value = value

	def __str__(self):
		return repr(self.value)


class MailboxURL:


	url    = ""
	user   = ""
	passwd = ""
	host   = ""
	port   = 0
	path   = ""

	def __init__(self,url):
		self.url = url
		self._parse()


	def _parse(self):
		m = re.match('imap://(.+):(.+)@(.+):([0-9]+)/(.+)',self.url)
		if m is None:
			raise InvalidMailboxURL('regex did not match')

		self.user   = m.group(1)
		self.passwd = m.group(2)
		self.host   = m.group(3)
		self.port   = int(m.group(4))
		self.path   = m.group(5)

	def __str__(self):
		return ("url:\t%s\n" % self.url) + \
		       ("user:\t%s\n" % self.user) + \
		       ("passwd:\t%s\n" % self.passwd) + \
		       ("host:\t%s\n" % self.host) + \
		       ("port:\t%i\n" % self.port) + \
		       ("path:\t%s\n" % self.path)
		       
		
