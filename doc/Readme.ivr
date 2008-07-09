
**********************
* IVR plug-in README *
**********************


Description:
------------

The 'ivr' plug-in enables SEMS to execute Python scripts 
implementing some application. If you need more information
concerning the developement of applications, please also have a
look at the python section of the HOWTO under following link:
http://www.iptel.org/howto/sems_application_development_tutorial

Configuration file ivr.conf:
----------------------------

script_path: path to the script repository. All the scripts 
             included in this path will be pre-loaded at startup.

Note: Pre-loaded IVR scripts are registered and thus reachable
      just like every other SEMS application plug-in.

For security reasons, only pre-loaded scripts can be executed.


How to select which Python script will be executed:
---------------------------------------------------

If the application determined by the normal application selection 
(sems.conf application=xyz) is "ivr", the script is executed which 
is named as the username. 
Example: R-URI 123@sems.iptel.org starts <script_path>/123.py


Troubleshooting:
----------------

- How to i know which scripts have been pre-loaded:

  Look at the log file when running with full debug infos.
  You will see at the beginning some similar entries:

(25110) DEBUG: Ivr-Python: Python-Ivr logging started
(25110) DEBUG: onLoad (Ivr.cpp:348): ** IVR compile time configuration:
(25110) DEBUG: onLoad (Ivr.cpp:349): **     built with PYTHON support.
(25110) DEBUG: onLoad (Ivr.cpp:352): **     Text-To-Speech enabled
(25110) DEBUG: onLoad (Ivr.cpp:357): ** IVR run time configuration:
(25110) DEBUG: onLoad (Ivr.cpp:358): **     script path:         '../apps/test_ivr'
(25110) DEBUG: onLoad (Ivr.cpp:374): directory '../apps/test_ivr' opened
(25110) INFO: onLoad (Ivr.cpp:401): Application script registered: test_ivr.

  This means that the script 'test_ivr.py' has been loaded successfully.


- My script won't load:

  Look at the debug information present at start time, some libraries 
  may be missing in which case you should set the proper PYTHONPATH
  before starting SEMS.


IVR API quickref:
-----------------


 globals: getHeader(String headers, String name)
             get header with name from headers

          getSessionParam(String headers, String name)
            get session parameter with name from headers
            (parameter from P-Iptel-Param)

          log(String str)
              log using sems' log facility

          createThread(Callable thread)
              create a thread. Only to be used in module 
              initialization code (no effect afterwards)

	  AUDIO_READ, AUDIO_WRITE (IvrAudioFile::open, fpopen)

	  SEMS_LOG_LEVEL (start log level)

class IvrDialogBase:

    # Event handlers
    def onStart(self): # SIP dialog start
        pass

    def onBye(self): # SIP dialog is BYEd
        pass
    
    def onSessionStart(self): # audio session start
        pass

    def onEmptyQueue(self): # audio queue is empty
        pass
    
    def onDtmf(self,key,duration): # received DTMF
        pass

    def onSipReply(IvrSipReply r):
    	pass

    def onSipRequest(IvrSipRequest r):
    	pass

    # Session control
    def stopSession(self): # stop everything
        pass

    def bye(self): # BYEs (or CANCELs) the SIP dialog
        pass


    # Media control
    def enqueue(self,audio_play,audio_rec): # add something to the playlist
        pass

    def flush(self): # flushes playlist
        pass

    # Call datas / control

    # get only property wrapping AmSipDialog AmSession::dlg.
    # only its properties should be exposed.
    dialog

    # B2BUA

    # if true, traffic will be relayed
    # transaprently to the other side
    # if this is 'True' at the beginning 
    # of the session, the Caller's INVITE
    # will be relayed to the callee, without
    # having to use connectCallee()
    B2BMode = False

    # call given party as (new) callee
    # local_party and local_uri are optional:
    #  if not present, the From of caller leg will be used 
    #  for the From of the callee leg.
    # Another options is connectCallee(None), then the callee 
    # of the initial caller request is connected
    # 
    # remote_party and local_party will be used as To/From headers.
    # remote_uri and local_uri is only provided for the application.
    #
    # Examples: 
    #  self.connectCallee(None)
    #   # connect To of caller leg with From of caller leg
    #  
    #  self.connectCallee('<sip:conference@serverip>', 'sip:conference@serverip')
    #   # connect conference@serverip with From of caller leg
    #  
    #  self.connectCallee('<sip:otheruser@domain>', 'sip:otheruser@domain',\
    #                     'FunkyApp <sip:funkyapp@domain>', 'sip:funkyapp@domain')
    #   # connect otheruser@domain from funkyapp.
    def connectCallee(self,remote_party,remote_uri,local_party,local_uri):
        pass

    # terminate the callee's call
    def terminateOtherLeg(self):
        pass

    # terminate our call
    def terminateLeg(self):
        pass

    # start a new audio session with the caller
    # sends a re-INVITE if needed.
    def connectAudio(self):
        pass

    # end the audio session
    # sends a re-INVITE if needed to reconnect to the current callee
    def disconnectAudio(self):
    	pass

    # B2BUA Event handlers
    # some other handlers...

class IvrUAC: 
      # make a new outgoing call
      def dialout(str user, str app_name, str r_uri, 
		       str from, str from_uri, str to)

# see AmAudioMixIn.h
class IvrAudioMixIn: 
      #
      # initialize with two audio devices, interval s, mixing level l, and 
      # the finish_b_while_mixing flag (optional, default false)
      def init(IvrAudio audio_a, IvrAudio audio_b, int s, double l [, int finish])

class AmAudioFile:
      #"open the audio file"
      def open(str filename, int open_mode [, bool is_tmp])
      #"open the audio file"
      def fpopen(str filename, int open_mode, File fp)
      # "close the audio file"
      def close()
      # "rewind the audio file"
      def rewind()
      #   "returns the recorded data size"
      int getDataSize()
      #   "set the maximum record time in millisecond"
      def setRecordTime(int record_time)
      #   "creates a new Python file with the actual file"
      #   " and eventually flushes headers (audio->on_stop)"
      def exportRaw()
      #   "text to speech"
      def tts(str text)

class IvrSipRequest: 
# properties are read-only
      str method
      str user
      str domain
      str dstip
      str port

      str r_uri
      str from_uri
      str from
      str to
      str callid
      str from_tag
      str to_tag
      str route
      str next_hop
      int cseq
      str body
      str hdrs

class IvrSipReply:
# properties are read-only
      int code
      str reason
      str next_request_uri
      str next_hop
      str route
      str hdrs
      str body
      str remote_tag
      str local_tag
      int cseq      

