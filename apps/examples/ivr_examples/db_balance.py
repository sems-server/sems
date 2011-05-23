# very simple example for db access and tts
#
#table for this with
# mysql> create database business;
# mysql> create table business.accounts (user VARCHAR(100), bal_int INT, bal_decimal INT);
#
# set TTS=y in apps/ivr/Makefile.defs, or rewrite with 
# enqueuing the parts of the sentence, like
#  self.ttsfile1 = IvrAudioFile()
#  self.ttsfile1.open("your_account_balance.wav", AUDIO_READ)
#  self.enqueue(self.ttsfile1, None)
#  self.ttsfile2 = IvrAudioFile()
#  self.ttsfile2.open("%i.wav"%int(res[0][0]), AUDIO_READ)
#  self.enqueue(self.ttsfile2, None)
#  etc...
#

from log import *
from ivr import *

import MySQLdb
db = MySQLdb.connection(host="127.0.0.1", user="root", passwd="sa07", db="business")
# or, when using config file db_balance.conf:
#db = MySQLdb.connection(host=config["db_host"], user=config["db_user"], passwd=config["db_pwd"], db=config["db_db"])

class IvrDialog(IvrDialogBase) :
    ttsfile = None

    def onSessionStart(self) :
        db.query("select bal_int, bal_decimal from accounts where user='%s'" % self.dialog.user)
        r = db.store_result()
        res = r.fetch_row(1)
        if len(res):            
            self.ttsfile = IvrAudioFile().tts("Your account balance is %i dollars and %i cents" % \
                                                  (int(res[0][0]), int(res[0][1])))
        else:
            self.ttsfile = IvrAudioFile().tts("Sorry, I do not know your account balance.")
        self.enqueue(self.ttsfile, None)
    
    def onEmptyQueue(self):
        self.bye()
        self.stopSession()
