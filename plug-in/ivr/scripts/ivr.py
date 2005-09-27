#!/usr/bin/env python

# path for wav files
wav_path = "/home/rco/src/cvs/sems/new_audio/wav/ivr/"
# path for received messages
messages_path = "/tmp/ivr/"
x=100


import os
import ivr
import re

# function returns name from URI
def get_name(str):
	user_expr_ext = re.compile('<sip:[\w&=+$,;?/-_.!~*()\']+@')
	user_expr = re.compile(':[\w&=+$,;?/-_.!~*()\']+[^@]')
	m = user_expr_ext.search(str)
	if m:
		user_temp = user_expr.search(m.group())
		user = re.search('[^:].+',user_temp.group())
	if user:
		return user.group()

print "IVR"

caller = get_name(ivr.getFrom())
callee = get_name(ivr.getTo())

print "Caller:", caller
print "Callee:", callee

print "play invitation"
ivr.play(wav_path + "ivr_invitation.wav")
print "record message"
if os.path.isdir(messages_path + callee) != True:
	os.mkdir(messages_path + callee)
ivr.record(messages_path + callee + "/" + callee + str(ivr.getTime()) + ".wav", 60)
      
