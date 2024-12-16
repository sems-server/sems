# -*- coding: utf-8 -*-

import sys

sys.path.append("/usr/local/lib/sems/plug-in/")

from log import *
from ivr import *

# states
connect = 1
connect_failed = 2

HINT_TIMER = 1
HINT_TIMEOUT = 10  # seconds until hint msg is played


class IvrDialog(IvrDialogBase):
    # messages to be played to the caller
    test1_msg = None
    test2_msg = None
    test3_msg = None

    # initial state
    state = None
    # entered keys
    keys = ""

    def sessionInfo(self):
        debug("IVR Session info:")
        debug(" user:        " + self.dialog.user)
        debug(" domain:      " + self.dialog.domain)

    def onSessionStart(self):
        self.sessionInfo()
        self.setNoRelayonly()

        self.sound_test1 = IvrAudioFile()
        self.sound_test1.open(config["test1_msg"], AUDIO_READ)

        self.sound_test2 = IvrAudioFile()
        self.sound_test2.open(config["test2_msg"], AUDIO_READ)

        self.sound_test3 = IvrAudioFile()
        self.sound_test3.open(config["test3_msg"], AUDIO_READ)

        self.enqueue(self.sound_test1, None)
        # self.enqueue(self.sound_test2, None)
        self.enqueue(self.sound_test3, None)

        self.setTimer(HINT_TIMER, HINT_TIMEOUT)

    def onBye(self):
        debug("onBye() fired")
        self.stopSession()

    def onDtmf(self, key, duration):
        if key == 1:
            debug("Caught key: " + str(key) + " Duration: " + str(duration))
            debug("ENDING CALL because we caught specific key :)")
            self.bye()

    def onEmptyQueue(self):
        debug("onEmptyQueue() fired")
        #if self.state == connect_failed:
        #    debug("ENDING CALL because state connect_failed has been set!")
        #    self.bye()
        #    self.stopSession()

    def onTimer(self, id):
        if id == HINT_TIMER:
            debug("ENDING CALL because TIMEOUT has been hit!")
            self.bye()
