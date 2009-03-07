#!/usr/bin/python

#
# QT webconf UI
# This was meant as a UI for webconference on the Nokia Internet Tablet,
# but it turns out that xmlrpc over https every second
# uses too much processing power (50-80% cpu) for that device
#
# (c) 2009 Stefan Sayer <sayer at iptel.org>
# License: GPLv3
#
import sys
import random
from PyQt4 import QtCore, QtGui
from PyQt4.QtGui import *
from PyQt4.QtCore import *

from xmlrpclib import *
from threading import *

from conftable import * 
from participant import *
from callbox import *
from account import *

# URI for XMLRPC webconference control, set to your server
CONTROL_URI="https://webconference.iptel.org/control"

# refresh in ms
REFRESH_INTERVAL=1000

HAS_ACCOUNT=False
try:
	from accountconfig import *
	HAS_ACCOUNT=True
except:
	print "accountconfig.py not found - will ask for account to make calls"
	pass

class named_participant(Ui_participant):
	def __init__(self, parent, title, callid, id):
		self.callid = callid
		self.id = id
		self.setupUi(parent)
		self.parent = parent
		self.tag = ""
		parent.setWindowTitle(title)
		QtCore.QObject.connect(self.bt_ciao, QtCore.SIGNAL("clicked()"), self.ciao_clicked)		
		QtCore.QObject.connect(self.cb_muted, QtCore.SIGNAL("stateChanged(int)"), self.muted_changed)
		
	def ciao_clicked(self):
		self.bt_ciao.emit(QtCore.SIGNAL("ciao(int)"), self.id)
		
	def status2text(self, s):
		return { 0:    "Disconnected",
         		 1:    "Connecting",
         		 2:    "Ringing",
         		 3:    "Connected",
         		 4:    "Disconnecting",
         		 5:    "Finished"}[s]
		
	def set_status(self, status, hint, muted):
		self.l_status.setText(self.status2text(status))
		self.l_status.setToolTip(hint)
		if muted:
			self.cb_muted.setCheckState(Qt.Checked)
		else:
			self.cb_muted.setCheckState(Qt.Unchecked)
	
	def muted_changed(self, i):
		print "callid is" , self.callid
		self.cb_muted.emit(QtCore.SIGNAL("mute(int,bool)"), self.id, self.cb_muted.isChecked())
		
		
class StartQT4(QtGui.QMainWindow):
	participants = []
	roomname = ""
	adminpin = ""	
	s = None
	last_res = None
	
	def __init__(self, parent=None):
		QtGui.QWidget.__init__(self, parent)
		self.ui = Ui_MainWindow()
		self.ui.setupUi(self)
		self.show()

		if HAS_ACCOUNT:		
			self.call_domain = DOMAIN
			self.call_user = USER
			self.call_pwd = PASSWORD
			self.call_auth_user = AUTH_USER
		else:
			dlg = QtGui.QDialog(self)
			dlg_cb = Ui_account()
			dlg_cb.setupUi(dlg)
			if dlg.exec_() == QDialog.Rejected:
				raise "well, I need a SIP account to make calls"
		
			self.call_domain = str(dlg_cb.e_domain.text())
			self.call_pwd = str(dlg_cb.e_pwd.text())
			self.call_user = str(dlg_cb.e_user.text())
			self.call_auth_user = self.call_user
		
		QtCore.QObject.connect(self.ui.buttonNew, QtCore.SIGNAL("clicked()"), self.new_call)
		self.s = ServerProxy(CONTROL_URI)
		print "server has %d running calls " % self.s.calls()
		for i in range(10):
			self.roomname = ""
			for n in range(6):
				self.roomname+=str(random.randint(0,9))
			code, result, adminpin, serverstatus = self.s.roomCreate(self.roomname)
			print "server status: %s " % serverstatus
			if code == 0:
				self.adminpin = adminpin
				break
		if self.adminpin == "":
			raise "oh, could not get a free room :("
		
		print "roomname is %s, adminpin is %s "% (self.roomname, self.adminpin)
		self.ui.label.setText("iptel.org\nwebconference\nhttps://webconference.iptel.org\n\nroom: %s    adminpin:%s\n\n" 
		                      "to dial in call sip:conference@iptel.org \nand type %s*"
		   %(self.roomname, self.adminpin, self.roomname))

		self.timer = QtCore.QTimer(self)
		self.connect(self.timer, QtCore.SIGNAL("timeout()"), self.timer_hit)
		self.timer.start(REFRESH_INTERVAL)
		
	def timer_hit(self):
		res = self.s.roomInfo(self.roomname, self.adminpin)
		if res[0] != 0:
			print "oh my god, can't see this room!"
			return
		
		code, reason, participants, serverstatus = res
		if participants == self.last_res:
			return #optimize a bit
		
		self.last_res = participants
		
		for part in participants:
			call_tag, number, status, reason, muted = part		
			found = False
			for p in self.participants:
				if p.callid == call_tag:
					p.set_status(status, reason, muted)
					found = True
					break
			if not found:
				p = self.createparticipantWidget(number, call_tag)
				p.set_status(status, reason, muted)
				
	def createparticipantWidget(self, name, callid):
		w = QtGui.QDockWidget("someone", self.ui.frame_main)
		part = named_participant(w, name, callid, len(self.participants))
		QtCore.QObject.connect(part.bt_ciao, QtCore.SIGNAL("ciao(int)"), self.part_ciao)
		QtCore.QObject.connect(part.cb_muted, QtCore.SIGNAL("mute(int,bool)"), self.part_muted)
		self.addDockWidget(random.choice([QtCore.Qt.RightDockWidgetArea, QtCore.Qt.LeftDockWidgetArea, QtCore.Qt.TopDockWidgetArea]), w)
		w.setFloating(True)
		w.show()
		self.participants = self.participants + [ part ]
		return part
		
	def new_call(self):
		print "a new call."
		dlg = QtGui.QDialog(self)
		dlg_cb = Ui_callbox()
		dlg_cb.setupUi(dlg)
		if dlg.exec_() == QDialog.Rejected:
			return
		
		print "now calling %s " % dlg_cb.num.text() 
		res = self.s.dialout(self.roomname, self.adminpin, str(dlg_cb.num.text()),
				self.call_user, self.call_domain, self.call_auth_user, self.call_pwd)
		if res[0] != 0:
			print "oh, my dear, calling failed with code %d " % res[0]
			return
		code, result, callid, serverinfo = res	
		print "code %d result %s " % (code, result)
		print "serverinfo is %s " % serverinfo 
						
		self.createparticipantWidget(dlg_cb.num.text(), callid)
		
	def part_ciao(self, id):
		print "ciao: ", id 
		self.s.kickout(self.roomname, self.adminpin, self.participants[id].callid)
		
	def part_muted(self, id, s):
		print "mute: ", id, " is ", s
		if s:
			self.s.mute(self.roomname, self.adminpin, self.participants[id].callid)
		else:
			self.s.unmute(self.roomname, self.adminpin, self.participants[id].callid)
		
		


if __name__ == "__main__":
	app = QtGui.QApplication(sys.argv)
	myapp = StartQT4()
	myapp.show()
	sys.exit(app.exec_())

