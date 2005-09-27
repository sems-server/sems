#!/usr/bin/env python

# path for wav files
wav_path = "/home/ilk/answer_machine/wav/ivr/"
# path for received messages
messages_path = "/tmp/ivr/"
x=100


import os
import ivr
import re
print "Python: Try to redirect"
ret = ivr.redirect("<sip:ivr@195.37.78.148>")
if ret :
   print "Python: Redirected"
else:
   print "Python: Not redirected"



