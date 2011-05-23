# 
# ivr b2bua script example
#
from log import *
from ivr import *

WELLCOME_MSG = "wav/default_en.wav"
CALLEE_URI = "sip:music@iptel.org"

class IvrDialog(IvrDialogBase):
    
    def onSessionStart(self):

        info("starting b2bua test ...")

        self.setNoRelayonly()
        
        self.welcome_msg = IvrAudioFile()
        self.welcome_msg.open(WELLCOME_MSG, AUDIO_READ)
        self.enqueue(self.welcome_msg,None)

    def onEmptyQueue(self):
        
        info("connecting to To: " + CALLEE_URI + " R-URI: " + CALLEE_URI)
        info("\n\n\n original headers are: ---->%s<----\n\n\n" % self.invite_req.hdrs)
        self.invite_req.hdrs += "P-SomeMoreFunky: headervalue\r\n"

        self.disconnectMedia()
        self.mute()

        self.connectCallee(CALLEE_URI, CALLEE_URI)

        return

    def onBye(self):		
        self.stopSession()
