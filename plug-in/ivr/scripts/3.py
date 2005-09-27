#!/usr/bin/env python

import ivr
import time

class BeatBox:
	def __init__(self):
	self.number_path = "/home/ssa/ivr/bbox/numbers/"
	self.bbox_path  = "/home/ssa/ivr/bbox/"
	self.lastchar = -1
	self.bbox_mode = 0

	def onDTMF(self, key):
	print "onDTMF, key = ", key
	print "lastchar = ", self.lastchar
	if self.lastchar == 0 and key == 0 :
		self.bbox_mode = not self.bbox_mode
	if self.bbox_mode:
		ivr.enqueueMediaFile(self.bbox_path + "b"+str(key) +".wav")
	else:
		ivr.enqueueMediaFile(self.number_path + str(key) +".wav")       
	self.lastchar = key

	def onMediaQueueEmpty(self):
		print "media ran out..."
		#ivr.enqueueMediaFile(self.bbox_path + "numbers/0.wav")

print "IVR"
print "IVIVI"

b = BeatBox()

def onDTMF_m(key):
	b.onDTMF(key)
def onMQE_m():
	b.onMediaQueueEmpty()

ivr.setCallback(onDTMF_m, "onDTMF")
ivr.setCallback(onMQE_m, "onMediaQueueEmpty")
ivr.startRecording("/home/ssa/ivr/bbox/recorded/" + ivr.getFrom() + " - " + time.ctime() + ".wav")
ivr.enableDTMFDetection()
ivr.enqueueMediaFile("/home/ssa/ivr/bbox/intro1.wav")
ivr.sleep(60)

