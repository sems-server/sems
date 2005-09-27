#!/usr/bin/env python

wav_path = "/home/ilk/answer_machine/wav/horoscope/"
x=100

import os
import ivr
print "Try horoscope "
x=ivr.playAndDetect(wav_path + "hor_invitation.wav",60000)
if x == 0:
   ivr.play(wav_path + "hor_1.wav")
if x == 1:
   ivr.play(wav_path + "hor_2.wav")
if x == 2:
   ivr.play(wav_path + "hor_3.wav")
if x == 3:
   ivr.play(wav_path + "hor_4.wav")
if x == 4:
   ivr.play(wav_path + "hor_5.wav")
if x == 5:
   ivr.play(wav_path + "hor_6.wav")
if x == 6:
   ivr.play(wav_path + "hor_7.wav")
if x == 7:
   ivr.play(wav_path + "hor_8.wav")
if x == 8:
   ivr.play(wav_path + "hor_9.wav")
if x == 9:
   ivr.play(wav_path + "hor_10.wav")
if x == 10:
   ivr.play(wav_path + "hor_11.wav")
if x == 11:
   ivr.play(wav_path + "hor_12.wav")
