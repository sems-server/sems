import os,base64,time
from imaplib import IMAP4

from MailboxURL import *


READ_BUFSIZE = 4096


def IMAPCALL(call_res):

	(t,d) = call_res
	if t != 'OK':
		raise IMAP4.error(d[0])
	return (t,d)


class IMAP4_MsgBODY:

	uid   = ''
	parts = []
	ct    = ''
		
	def __init__(self,imap,uid):

		self.uid   = ''
		self.parts = []
		self.ct    = ''

		(t,d) = imap.uid('FETCH',uid,'(BODY)')
		if d[0] is None:
			raise IMAP4.error('No message with UID "%s" has been found' % uid)
		self._parse_body_desc(d[0])

	def part_ct(self,part):

		return '%s/%s' % (self.parts[part][0],self.parts[part][1])

	def fetch2file(self,imap,part,filename=None):

		(t,d) = imap.uid('fetch',self.uid,'(BODY.PEEK[%i])' % (part+1))
		if (t != "OK") or (len(d[0]) != 2):
			raise IMAP4.error("could not retrieve part %s/%i" \
					  % (self.uid, part+1))
			
		msg = d[0][1]
		msg = base64.decodestring(msg)
		if filename is None:
			out_file = os.tmpfile()
		else:
			out_file = open(filename,'w+')
			
		out_file.write(msg)
		out_file.seek(0)

		if filename is None:
			return out_file
		else:
			out_file.close()
			return None


	def _parse_body_desc(self,desc):

		(_dummy,s) = self._parenthesis2seq(desc)[1]
		
		self.uid = s[1]

		if type(s[3][0]) is not str:

			# Multipart message
			for i in s[3]:
				if type(i) is str:
					self.ct = i
					break
				self.parts.append(i)
		else:
			self.parts.append(s[3])
			self.ct = s[3][0]


	def _parenthesis2seq(self,s,ibeg=0):

		seq = []
		i = elmt_beg = ibeg

		while i < len(s):

			if s[i] == '(':
				(i,tmp_seq) = self._parenthesis2seq(s,i+1)
				elmt_beg = i
				seq.append(tmp_seq)
			
			elif s[i] == ')':
				if elmt_beg < i:
					seq.append(s[elmt_beg:i])
				return (i+1,seq)

			elif s[i] == ' ':
				if elmt_beg < i:
					if s[elmt_beg] == '"':
						seq.append(s[elmt_beg+1:i-1])
					else:
						seq.append(s[elmt_beg:i])
				i = i+1
				elmt_beg = i
			else:
				i = i+1

		return (i,seq)



class IMAP4_Mailbox:

	url  = None
	imap = None

	def __init__(self,url):

		self.url = None
		self.url = MailboxURL(url)

	def __del__(self):

		if self.imap != None:
			self.imap.logout()
			self.imap = None
		

	def createBox(self):

		self._login()
		debug("Creating account: " + \
		      repr(IMAPCALL(self.imap.create(self.url.path))))


	def deleteBox(self):

		self._login()
		debug("Deleting account: " + \
		      repr(IMAPCALL(self.imap.delete(self.url.path))))


	def uploadMsg(self,msg):
		self._login()
		IMAPCALL(self.imap.append(self.url.path,None,time.gmtime(),msg))

	def deleteMsg(self,uid):

		self._login()
		IMAPCALL(self.imap.select(self.url.path))
		IMAPCALL(self.imap.uid('STORE',uid,'+flags', '(\\Deleted)'))
		IMAPCALL(self.imap.expunge())
		IMAPCALL(self.imap.close())

	def saveMsg(self,uid):

		self._login()
		IMAPCALL(self.imap.select(self.url.path))
		IMAPCALL(self.imap.uid('STORE',uid,'+flags','(\\seen)'))
		IMAPCALL(self.imap.close())

	def getWavMsgList(self,search_criterion):

		self._login()
		IMAPCALL(self.imap.select(self.url.path))

		(t,d) = self.imap.uid('SEARCH',search_criterion)
		IMAPCALL((t,d))

		msg_list = []
		for i in d[0].split():
			body = IMAP4_MsgBODY(self.imap,i)
			for p in range(len(body.parts)):
				if body.part_ct(p).upper() == 'AUDIO/X-WAV':
					msg_list.append(body.uid)

		IMAPCALL(self.imap.close())
		return msg_list


	def downloadWAV(self,msg_uid):
		
		self._login()
		IMAPCALL(self.imap.select(self.url.path))
		body = IMAP4_MsgBODY(self.imap,msg_uid)

		fp = None
		for p in range(len(body.parts)):
			if body.part_ct(p).upper() == 'AUDIO/X-WAV':
				fp = body.fetch2file(self.imap,p)
				break

		IMAPCALL(self.imap.close())
		return fp
		

	def downloadWAVs(self,search_criterion,file_prefix):

		self._login()
		IMAPCALL(self.imap.select(self.url.path))
		
		(t,d) = self.imap.uid('SEARCH',search_criterion)
		IMAPCALL((t,d))

		msg_list = []
		for i in d[0].split():

			body = IMAP4_MsgBODY(self.imap,i)
		
			for p in range(len(body.parts)):
			
				if body.part_ct(p).upper() == 'AUDIO/X-WAV':
					
					filename = file_prefix + '-' + body.uid + '.wav'
					msg_list.append(filename)
					body.fetch2file(self.imap,p,filename)
					
		IMAPCALL(self.imap.close())
		return msg_list

	def _login(self):

		if self.imap != None:
			return

		self.imap = IMAP4(self.url.host,self.url.port)
		try:
			IMAPCALL(self.imap.login(self.url.user,self.url.passwd))
		except:
			self.imap = None
			raise
		
