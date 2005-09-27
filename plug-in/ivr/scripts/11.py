#!/usr/bin/env python

# path for wav files
wav_path = "/home/ilk/answer_machine/wav/ivr/"
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

# if user is owner of mailbox
# user can listed, delete or choose next message
if  caller == callee:
   if os.path.isdir(messages_path + callee):
      for files in os.listdir(messages_path + callee):
         while x > 0:
            if x > 3:
               x=ivr.playAndDetect(wav_path + "ivr_instruction.wav",60000)
            if x == 1:
               # listen message
               ivr.play(messages_path + callee + "/" + files)
               x=100
            if x == 2:
               # delete message
               os.remove(messages_path + callee + "/" + files)
               x=1
               ivr.play(wav_path + "ivr_next_message.wav")
               break
            if x == 3:
               # listen next message
               x=1
               ivr.play(wav_path + "ivr_next_message.wav")
               break
   # no new messages
   if x > 0:
      ivr.play(wav_path + "ivr_no_messages.wav")
# if user is not user of mailbox
# user can left message
else :
   print "play invitation"
   ivr.play(wav_path + "ivr_invitation.wav")
   print "record message"
   if os.path.isdir(messages_path + callee) != True:
      os.mkdir(messages_path + callee)
   ivr.record(messages_path + callee + "/" + callee + str(ivr.getTime()) + ".wav", 60)
      
