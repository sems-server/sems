import base64,time,os,sip

from py_sems_log import *
from py_sems import *
from py_sems_lib import *

class PySemsScript(PySemsDialog):

	def __init__(self):
		
		debug("***** __init__ *******")
		PySemsDialog.__init__(self)
		self.initial_req = None
		self.ann = None
		sip.settracemask(0xFFFF)

	def onInvite(self,req):


		print "----------------- %s ----------------" % self.__class__
		
		ann_file = self.getAnnounceFile(req)
		self.ann = AmAudioFile()

		try:
			self.ann.open(ann_file)

			self.initial_req = AmSipRequest(req)
			debug("dlg.local_tag: %s" % self.dlg.local_tag)
		
			debug("***** onInvite *******")
			(res,sdp_reply) = self.acceptAudio(req.body,req.hdrs)
			if res < 0:
				self.dlg.reply(req,500)
		
			debug("res = %s" % repr(res))
			debug("sdp_reply = %s" % sdp_reply)
		
			if self.dlg.reply(req,183,"OK","application/sdp",sdp_reply,"") <> 0:
				self.setStopped()
		except:
			self.dlg.reply(req,500,"File not found","","","")
			self.ann = None
			self.setStopped()
			raise
		
		
  	def onSessionStart(self,req):

  		debug("***** onSessionStart *******")
		PySemsDialog.onSessionStart(self,req)

		self.localreq = AmSipRequest(req)
		self.setOutput(self.ann)


	def onCancel(self):
		
		debug("***** onCancel *******")

		self.dlg.reply(self.initial_req,487,"Call terminated","","","")
		self.setStopped()


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

			code = getHeader(self.localreq.hdrs,"P-Final-Reply-Code")
			reason = getHeader(self.localreq.hdrs,"P-Final-Reply-Reason")

			if reason == "":
				reason = "OK"
			
			code_i = 400
			try:
				code_i = int(code)
				if (code_i < 300) or (code_i>699):
					debug("Invalid reply code: %d",code_i)
			except:
				debug("Invalid reply code: %s",code)
	
			debug("Replying %d %s" % (code_i, reason))
			self.dlg.reply(self.localreq, code_i, reason, "", "", "")
			self.setStopped()
			return
		
		PySemsDialog.process(self,ev);
		return

			
