#
# just playing file
#
from log import *
from ivr import *

class IvrDialog(IvrDialogBase) :

    def onSessionStart(self) :
        self.audio_pl = IvrAudioFile()
        self.audio_pl.open("wav/default_en.wav", AUDIO_READ)

        self.enqueue(self.audio_pl,None)
        
    def onBye(self):
        self.stopSession()

    def onEmptyQueue(self):
        self.bye()
        self.stopSession()

    def onSipReply( self, reply ) :
        pass

    def onSipRequest( self, reply ) :
        pass
