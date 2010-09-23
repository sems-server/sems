# DB based Announcement application for SEMS

# Copyright 2007 Juha Heinanen
#
# This file is part of SEMS, a free SIP media server.
#
# SEMS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# Use, copying, modification, and distribution without written
# permission is not allowed. 

import os, MySQLdb

from log import *
from ivr import *

APPLICATION = 'announcement'

GREETING_MSG = 'greeting_msg'

class IvrDialog(IvrDialogBase):

    DB_HOST = 'localhost'
    DB_USER = 'sems'
    DB_PASSWD = ''
    DB_DB = 'sems'

    def __init__(self):

	try:
	    if config['mysql_server']:
		self.DB_HOST = config['mysql_server']
        except KeyError:
            pass

	try:
	    if config['mysql_user']:
		self.DB_USER = config['mysql_user']
        except KeyError:
            pass

	try:
	    if config['mysql_passwd']:
		self.DB_PASSWD = config['mysql_passwd']
        except KeyError:
            pass

	try:
	    if config['mysql_db']:
		self.DB_DB = config['mysql_db']
        except KeyError:
            pass

        try:
            self.db = MySQLdb.connect(host=self.DB_HOST,\
                                      user=self.DB_USER,\
                                      passwd=self.DB_PASSWD,\
                                      db=self.DB_DB)
        except MySQLdb.Error, e:
            error(APPLICATION + ": cannot open database: " +\
                  str(e.args[0]) + ":" + e.args[1])
            return False

        self.audio = dict()
        
        return True

    def onSessionStart(self, hdrs):

        if not self.__init__():
            self.bye()
            self.stopSession()
            
        self.language = getHeader(hdrs, "P-Language")

        if not self.language:
            self.language = "  "

        if not self.findAudioMsg(GREETING_MSG, 3):
            self.sendBye()
            return

        self.enqueue(self.audio[GREETING_MSG], None)
        
    def onEmptyQueue(self):

        if not self.queueIsEmpty():
            return
        
        self.sendBye()

        return
    
    def onBye(self):

        self.db.close()
        self.stopSession()
        return

    def sendBye(self):

        self.db.close()
        self.bye()
        self.stopSession()
        return

    def findAudioMsg(self, msg, start):

        wav = IvrAudioFile()

        try:

            cursor = self.db.cursor()

            if start > 2:

                cursor.execute("SELECT audio FROM user_audio WHERE application='" + APPLICATION + "' AND message='" + msg + "' AND domain='" +  self.dialog.domain + "' AND userid='" + self.dialog.user + "'")

                if cursor.rowcount > 0:
                    self.getFromTemp(cursor.fetchone()[0], msg, wav)
                    cursor.close()
                    return True

            if start > 1:

                cursor.execute("SELECT audio FROM domain_audio WHERE application='" + APPLICATION + "' AND message='" + msg + "' AND domain='" + self.dialog.domain + "' AND language='" + self.language + "'")

                if cursor.rowcount > 0:
                    self.getFromTemp(cursor.fetchone()[0], msg, wav)
                    cursor.close()
                    return True
            
            cursor.execute("SELECT audio FROM default_audio WHERE application='" + APPLICATION + "' AND message='" + msg + "' AND language='" + (self.language) + "'")

            if cursor.rowcount > 0:
                self.getFromTemp(cursor.fetchone()[0], msg, wav)
                cursor.close()
                return True
            else:
                error(APPLICATION + ": default " + msg + " is missing!")
                cursor.close()
                return False

        except MySQLdb.Error, e:
            error(APPLICATION + ": error in accessing database: " +\
                  str(e.args[0]) + ":" + e.args[1])
            return False
        
    def getFromTemp(self, audio, msg, wav):

        fp = os.tmpfile()
        fp.write(audio)
        fp.seek(0)
        wav.fpopen("tmp.wav", AUDIO_READ, fp)
        self.audio[msg] = wav
