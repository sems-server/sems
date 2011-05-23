#
# just recording to file
#
from log import *
from ivr import *

class IvrDialog(IvrDialogBase) :

    def onSessionStart(self) :

        self.audio_msg = IvrAudioFile()
        self.audio_msg.open("record.wav", AUDIO_WRITE)
        self.enqueue(None,self.audio_msg)
