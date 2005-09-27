#!/usr/bin/env python

wav_path = "/home/ilk/answer_machine/wav/horoscope/"
m=100
date=0

import os
import ivr
print "Try horoscope "
m = ivr.playAndDetect(wav_path + "enter_month.wav",60000)
if m >= 0:
   if m == 1 :
      m2 = ivr.detect(6000)
      if m2 >= 0 and m2 < 3:
         date = 1000 + m2 * 100
      else :
         date = m * 100
   else :
      if m == 0 :
         m2 = ivr.detect(6000)
         date = m2 * 100
      else :
         date = m * 100
   d = ivr.playAndDetect(wav_path + "enter_date.wav",60000)
   if d >= 0 and d < 4:
      d2 = ivr.detect(6000)
      if d2 >= 0:
         date += d * 10 + d2
      else :
         date += d 
   else :
      date += d
   if  121 <= date <= 131 or 201 <= date <= 219  :
      ivr.play(wav_path + "hor_1.wav")
   if  220 <= date <= 231 or 301 <= date <= 319 :
      ivr.play(wav_path + "hor_2.wav")
   if  320 <= date <= 331 or 401 <= date <= 418  :
      ivr.play(wav_path + "hor_3.wav")
   if  419 <= date <= 431 or 501 <= date <= 519 :
      ivr.play(wav_path + "hor_4.wav")
   if  520 <= date <= 531 or 601 <= date <= 620 :
      ivr.play(wav_path + "hor_5.wav")
   if  621 <= date <= 631 or 701 <= date <= 721 :
      ivr.play(wav_path + "hor_6.wav")
   if  722 <= date <= 731 or 801 <= date <= 822 :
      ivr.play(wav_path + "hor_7.wav")
   if  823 <= date <= 831 or 901 <= date <= 922  :
      ivr.play(wav_path + "hor_8.wav")
   if  923 <= date <= 931 or 1001 <= date <= 1022  :
      ivr.play(wav_path + "hor_9.wav")
   if  1023 <= date <= 1031 or 1101 <= date <= 1121  :
      ivr.play(wav_path + "hor_10.wav")
   if  1122 <= date <= 1131 or 1201 <= date <= 1220 :
      ivr.play(wav_path + "hor_11.wav")
   if  1221 <= date <= 1231 or 101 <= date <= 120  :
      ivr.play(wav_path + "hor_12.wav")
