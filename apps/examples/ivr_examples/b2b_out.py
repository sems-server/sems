# (outgoing) ivr b2bua example
#
#
from log import *
from ivr import *

class IvrDialog(IvrDialogBase):
    
    def onSessionStart(self):

        info("starting b2bua app ...")
        self.setNoRelayonly()        
        self.audio_msg = IvrAudioFile()
        self.audio_msg.open("/home/stefan/sub_nautilus.wav", AUDIO_READ)
        self.setTimer(1, 10)
        self.enqueue(self.audio_msg,None)

    def onEmptyQueue(self):
	return        

    def onTimer(self, timerid):
        self.disconnectMedia()
        debug('hello kitty')
        self.connectCallee(self.dialog.local_party,self.dialog.local_uri, \
                           self.dialog.remote_party,self.dialog.remote_uri,)
	return        

    def onBye(self):		

        self.stopSession()
