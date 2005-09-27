#!/usr/bin/env python

# path for wav files
wav_path = "/home/ilk/answer_machine/wav/ivr/"
# path for received messages
messages_path = "/tmp/ivr/"
x=100
ret=0


import os
import ivr
import re
x=ivr.playAndDetect(wav_path + "ivr_redirect.wav",60000)
print "Python: Try to redirect"
if x == 1:
   ret = ivr.redirect("<sip:ivr@195.37.78.148>")
if x == 2:
   ret = ivr.redirect("<sip:horoscope_date@195.37.78.148>")
if ret :
   print "Python: Redirected"
else:
   print "Python: Not redirected"
