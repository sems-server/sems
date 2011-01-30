webconference : conference with dial-in and dial-out over DI (xmlrpc)

Intro
-----
This conference module can do dial-in, dial-out and mixed 
dial-in/dial-out conferences.  It can be controlled over DI functions 
from other modules, and, for example, using the xmlrpc2di module, 
the conference rooms can be controlled via XMLRPC.

It implements conference rooms with dial-in and conference room entry
via DTMF, and authenticated dial-out.

For dial-in, usually the user is asked to enter the conference room 
number via keypad, followed by the star key.

In the configuration, a regular expression can be defined (direct_room_re),
which, if matched to the user part of the request uri, connects the caller 
directly to the conference room. direct_room_strip sets how many characters
should be stripped from the user part in order to obtain the conference room 
number.

Participants can be listed, kicked out, muted and unmuted over DI 
(using xmlrpc2di over XMLRPC).

You will probably want to load uac_auth and xmlrpc2di modules. 

There is some very simple feedback and call statistics functionality, which 
will save its results to a log file.

A master password can be set, which serves to retrieve room PINs (adminpins),
for site administrator or the like.

There is SIP Session Timer (RFC4028) support, which is configured in 
webconference.conf. By default, session timers are turned not enabled.

Room adminpin handling
----------------------
The adminpin is used so that only authorized users can access the conference control.
The adminpin can be completely disabled by setting ignore_pin in webconference.conf, 
in that case a specified adminpin is just ignored.

There is two modes, configured in the configuration file: private_rooms=yes and
private_rooms=no (which is the default).

For private_rooms=no:
  On all actions that inspect or modify a room, if the room specified with room/adminpin
  does not exist, by default the room is (re)opened with the specified adminpin. 
  This means that roomCreate() does not necessarily need to be called; if roomInfo() 
  is called with a new room name and adminpin, the room is created and the adminpin 
  is set.

For private_rooms=yes:
  The room has to be created with roomCreate, before anyone can enter the room by
  dialing in or by creating a call with dialout.

If a room exists and the adminpin is not set (for example if the room is created
by dial-in), the first call to roomInfo/dialout/kick/... with room/adminpin will
set the adminpin.

Participant ID
--------------

In some service environments, it may be necessary to determine in which conference
rooms a certain user is at the moment. For example, if the active conferences of a
user should be displayed on a web page, the conference bridge may be queried for the
room a user is in.

The findParticipant(string participant_id) function returns a list of rooms where the
participant with a given ID is in. This participant ID can be given with
 - a parameter to the dialout() function
 - a header or P-App-Param parameter on incoming calls (see participant_id_param and
   participant_id_header configuration in webconference.conf)

implemented DI functions
------------------------
All functions return as extra parameter the serverInfo, a status line showing the 
SEMS version, and current call statistics.

----
roomCreate(string room [, int timeout]):
   int code, string result, string adminpin

  if webconference is configured with support_rooms_timeout=yes, the room is deleted
  and participants are disconnected after <timeout>, if timeout parameter is present
  and timeout > 0.

  code/result:
         0    OK
         1    room already open
----
roomInfo(string room, string adminpin):
   int code, string result, list<participant> participants
   participant: string call_tag, string number, int status, string reason, int muted

  status:
         0    Disconnected
         1    Connecting
         2    Ringing
         3    Connected
         4    Disconnecting
	 5    Finished
   reason: e.g. "200 OK", "606 Declined", ...

  code/result:
         0    OK
         1    wrong adminpin
----
dialout(string room, string adminpin, string callee,
        string from_user, string domain,
        string auth_user, string auth_pwd [, headers [, callee_domain, [participant_id]]]) :
     int code, string result, string callid

   code/result:
          0     OK
          1     wrong adminpin
	  2     failed
----
kickout(string room, string adminpin, string call_tag) :
     int code, string result

   code/result:
          0     OK
          1     wrong adminpin
	  2     call does not exist in room
----
mute(string room, string adminpin, string call_tag) :
     int code, string result

   code/result:
          0     OK
          1     wrong adminpin
	  2     call does not exist in room
----
unmute(string room, string adminpin, string call_tag) :
     int code, string result

   code/result:
          0     OK
          1     wrong adminpin
	  2     call does not exist in room
----
changeRoomAdminpin(string room, string adminpin, string new_adminpin) : 
     int code, string result

   code/result: 
          0     OK
          1     wrong adminpin
----
serverInfo():
      string serverInfo
----
getRoomPassword(string master_pwd, string room)
    int code, string result
----
listRooms(string master_pwd)
    int code, string result
----
findParticipant(string participant_id)
    rooms: array of string
  find all rooms a participant is in
-----

additionally there is feedback functions to save call quality reports 
from users: vqRoomFeedback, vqCallFeedback, vqConferenceFeedback.
resetFeedback, flushFeedback can be used to manipulate the feedback files.

getRoomPassword and listRooms in only available if master password is set 
in webconference.conf


prompt suggestions: 
------------------
entering_conference: You are now entering the conference room.
first_participant: You are the first participant in the conference.
pin_prompt: Welcome to iptel dot org conference. Please enter your room number, followed by the pound key.
wrong_pin: The room number is not correct. Please try again.

+ the numbers (0.wav ... 9.wav )


webconference.iptel.org server
------------------------------

At the free SIP service iptel.org there is a SEMS webconference server 
running, which can be accessed through its rather simple web GUI at
 https://webconference.iptel.org/ and http://webconference.iptel.org/
and through its XMLRPC control URI
 https://webconference.iptel.org/control

see also:
--------
pyqt example gui client in 
 apps/webconference/pyqtgui
