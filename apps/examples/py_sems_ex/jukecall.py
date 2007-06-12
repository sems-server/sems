import base64,time,os,sip

from py_sems_log import *
from py_sems import *
from py_sems_lib import *

class MyB2ABEvent(PySemsB2ABEvent):
	def __init__(self, id):
		PySemsB2ABEvent.__init__(self,id)

class MyCalleeSession(PySemsB2ABCalleeDialog):
	def __init__(self, tag):
		debug("**** __init callee __ ****")
		AmB2ABCalleeSession.__init__(self, tag)
		self.ann=None
		#debug("**** tag = " + tag);

	def onPyB2ABEvent(self, ev):
		debug("***************************** callee PyB2ABEvent  ************************")
		if isinstance(ev, MyB2ABEvent):
			self.ann = AmAudioFile()
			self.ann.open("/tmp/test.wav")
			self.setOutput(self.ann)
			return

			
class PySemsScript(PySemsB2ABDialog):

	def __init__(self):
		
		debug("***** __init__ *******")
		PySemsB2ABDialog.__init__(self)
		self.initial_user = None
		self.initial_domain = None
		self.initial_fromuri = None
		self.ann = None
		sip.settracemask(0xFFFF)

	def onInvite(self, req):
		if len(req.user) < 2:
			self.dlg.reply(req,500,"Need a number to dial","","","")
			self.setStopped()
			return

		ann_file = self.getAnnounceFile(req)
		self.ann = AmAudioFile()
		try:
			self.ann.open(ann_file)
		except:
			self.dlg.reply(req,500,"File not found","","","")
			self.ann = None
			self.setStopped()
			raise
			
		PySemsB2ABDialog.onInvite(self,req)

  	def onSessionStart(self,req):
		self.setOutput(self.ann)
		self.initial_user = req.user
		self.initial_domain = req.domain
		self.initial_fromuri = req.from_uri

	def getAnnounceFile(self,req):

		announce_file = config["announce_path"] + req.domain + "/" + get_header_param(req.r_uri, "play") + ".wav"

		debug("trying '%s'",announce_file)
		if os.path.exists(announce_file):
			return announce_file

		announce_file = config["announce_path"] + req.user + ".wav"
		debug("trying '%s'",announce_file)
		if os.path.exists(announce_file):
			return announce_file

		announce_file = config["announce_path"] + config["announce_file"]
		debug("using default '%s'",announce_file)
		return announce_file


	def process(self,ev):

		debug("*********** PySemsScript.process **************")
		if isinstance(ev,AmAudioEvent):
		  if ev.event_id == AmAudioEvent.cleared:
			debug("AmAudioEvent.cleared")
			to = self.initial_user[1:len(self.initial_user)] + \
				"@" + self.initial_domain
			debug("to is " + to)
			debug("from is "+ self.initial_fromuri)
			self.connectCallee("<sip:"+to+">", "sip:"+to, \
				self.initial_fromuri, self.initial_fromuri)
			debug("connectcallee ok")
			return
		
		PySemsB2ABDialog.process(self,ev);
		return

	def createCalleeSession(self):
		print self.dlg.local_tag
		cs = MyCalleeSession(self.dlg.local_tag)
		print cs
		return cs

	def onDtmf(self, event, dur):
		debug("************ onDTMF: ********* " + str(event) + "," + str(dur))
		ev  = MyB2ABEvent(15)
		self.relayEvent(ev)
	
			
