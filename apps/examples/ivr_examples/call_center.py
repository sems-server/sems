# Skeleton of a call center style application that answers, plays beep_file
# to caller and then tries attendants on callee_list in round robin fashion
# as long as one of them replies

# Author Juha Heinanen <jh@tutpro.com>

import time

from log import *
from ivr import *

beep_file = "/var/lib/sems/audio/general/beep_snd.wav"
callee_list = ['sip:foo@test.tutpro.com', 'sip:test@test.tutpro.com']

beeping                   = 1
connecting                = 2
connected                 = 3

class IvrDialog(IvrDialogBase):

    def onSessionStart(self):

        self.callee_list = callee_list
        self.callee_index = 0
        self.setNoRelayonly()
        self.state = beeping
        self.audio_msg = IvrAudioFile()
        self.audio_msg.open(beep_file, AUDIO_READ)
        self.enqueue(self.audio_msg, None)

    def onBye(self):

	self.stopSession()

    def onEmptyQueue(self):

        if self.state == beeping:
            self.state = connecting
            self.connectTry()
            return

        return
    
    def onOtherReply(self, code, reason):

        debug('call_center: got reply: ' + str(code) + ' ' + reason)

        if self.state == connecting:

            if code < 200:
                return
            if code >= 200 and code < 300:
                self.flush()
                self.disconnectMedia()
                self.setRelayonly()
                self.state = connected
                debug('call_center: connected to ' + self.callee_uri)
                return
            if code >= 300:
                time.sleep(3)
                self.connectTry()
                return
        else:
            return

    def connectTry(self):

            self.callee_uri = self.callee_list[self.callee_index]
            self.callee_index = (self.callee_index + 1) % 2
            debug('call_center: trying to connectCallee ' + self.callee_uri)
            self.connectCallee(self.callee_uri, self.callee_uri)
