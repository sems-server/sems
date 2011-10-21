#
# just play a file whose name may be given in P-App-Param header
#

from log import *
from ivr import *
from p_app_param.p_app_param import *

class IvrDialog(IvrDialogBase):

    def onInvite(self, hdrs):
        if self.dialog.status_str == "Connected":
            debug("Received play re-INVITE")
            return True
        else:
            debug("Received play INVITE")
        self.params = P_App_Param(getHeader(hdrs, "P-App-Param"))
        self.file = self.params.getKeyValue("File")
        if not self.file:
            self.file = "/tmp/default.wav"
        return True
            
    def onSessionStart(self):
        self.announcement = IvrAudioFile()
        self.announcement.open(self.file, ivr.AUDIO_READ, False)
        self.enqueue(self.announcement, None)
        return
        
    def onEmptyQueue(self):
        if not self.queueIsEmpty():
            return
        self.bye()
        self.stopSession()
        return
    
    def onBye(self):
        self.stopSession()
        return

    def onSipRequest(self, hdrs):
        pass

    def onSipReply(self, reply):
        pass
