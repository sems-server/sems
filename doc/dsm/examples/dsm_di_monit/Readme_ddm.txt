DSM + monitoring + DI 
---------------------

This is a short sample that shows how to use the lastest
VoIP services application technologies from SEMS to rapidly 
create complex converged applications with minimal amount of
coding.

We will use the DSM state machine scripting to create a simple
conference application, in which the user is taken to the room
by interaction with an external web application. The focus lies
here on the media server part, thus we will not create a web 
interface, but show the functionality through some sample python
commands. 

Components used
---------------

Since 1.1 SEMS has a 'dsm' module, which allows to specify and 
implement applications in a simple state machine language. The DSM 
interpreter is extensible with modules, which can add conditions
and actions for the transitions between states in a DSM. 

Modules in SEMS can export an interface to other modules, so called
'DI' ('dynamic invocation') API, which has named functions, that 
take variant typed arguments. A DI API can be exported to the outside
world very easily by using the xmlrpc2di module, which offers an 
XMLRPC interface. For example, DSM exports a DI method to post events
to a DSM session.

The 'monitoring' module serves for general purpose call monitoring.
Applications can add specific attributes to the set which is being 
logged for every call. Monitoring exports is functions also through 
a DI interface, which can then be accessed for example from the 
outside through XMLRPC (using xmlrpc2di module). On the other hand 
there is a DSM module mod_monitoring, which allows a DSM to add 
attributes (monitoring.log()).



     DSM                         monitoring
+-----------+                   +----------+  
|           |                   |          |  
| O --> O   |  monitoring.log() |          |  
|       |   |   ======>         |          |  
|       |   |                   |          |  
|       v   |                   |          |  
|       O   |                   |          |  
+-----------+                   +----------+  
     ^                           ^
     | DI:                       |  DI: 
     |  postDSMEvent(...)        |   list(), listByFilter(), 
     +--------+  +---------------+   get()
              |  |
          +-----------+
          |           |
          | xmlrpc2di |
          |           |
          +-----------+
               ^
               |  XMLRPC:
               |   postDSMEvent(...)
               |   list(), listByFilter(), get()
               |

         +------------+
         |            |
         | web server |
         |  etc       |
         +------------+

Application
-----------

Lets have a look at the application (dsm_di_monitoring.dsm):

First we load the conference and the monitoring modules:
|    import(mod_conference)
|    import(mod_monitoring)

We denote the inital state with 'lobby' and when entering it,
we play a welcome message, and save the username (it's actually 
the callee user) and the position 'lobby' to the session's 
monitoring status:

|    initial state lobby 
|       enter { 
|   	   playFile(wav/welcome.wav) 
|	   monitoring.log(username, @user)
|	   monitoring.log(position, lobby)
|       };

Next, we define a state (now not an initial state) called 'room':
|     state room;

Events as sent by the postDSMEvent DI function have event parameters
(like other DSM events; for example for key press events, the key 
pressed is event parameter, or for timer timeout, the timer id is 
event parameter). Event parameters are accessed by the hash sign.
If we receive an event with the action 'take'

|    transition "lobby to room" lobby - eventTest(#action==take) /  {   

we first close the playlist to stop the welcome message from playing
if it still is:

|     flushPlaylist()

As playout type we set adaptive playout buffering; this is just for 
conference to sound smoothly in the presence of packet loss and jitter
(but we can ignore it for now):

|     conference.setPlayoutType(adaptive)

play a file to tell the user that we are going to the conference now, 
and join the conference:
|     playFile(wav/taken.wav) 
|     conference.join(#roomname)

Because we may have had empty playlist, we reconnect playing audio: 
|     connectMedia()

and finally, we update the position of the call to the monitoring log,
so that it is visible from the outside:

|     monitoring.log(position, #roomname)

and we change to the state 'room':
|    } -> room;

If we are in the room, and get moved to another room, we do similar things,
we just use a different prompt to play to the user:

|    transition "room change" room - eventTest(#action==take) /  {   
|       flushPlaylist()
|       playFile(wav/change.wav) 
|       conference.join(#roomname)
|       connectMedia()
|       monitoring.log(position, #roomname)
|    } -> room;

Note that we still stay in the state 'room', so this can be done several 
times.

If we want to stop the call from outside, we send an event with the action 'kick':

|    transition "kickout event" room - eventTest(#action==kick) / stop(true) -> end;

stop(true) sends a BYE and stops the session. We should not forget to stop the session if we receive a BYE as well:

|    transition "bye recvd" (lobby, room) - hangup / stop(false) -> end;
|    state end;


So, lets try this out. First we load the necessary modules and set as 
application to execute when a call comes in the DSM script:
sems.conf:
 load_plugins=wav;l16;ilbc;sipctrl;dsm;xmlrpc2di;monitoring
 application=dsm_di_monitoring

We tell dsm to load this dsm script and register it as 
application in SEMS:
dsm.conf:
 diag_path=../doc/dsm/examples/dsm_di_monit
 load_diags=dsm_di_monitoring
 register_apps=dsm_di_monitoring
 mod_path=../apps/dsm/mods/lib/

If we start SEMS from core/ directory, we also need to copy the wav 
files to the core/wav folder, because we have used e.g. wav/welcome.wav 
in the script: 
$ cp apps/dsm/doc/examples/dsm_di_monit/wav/*.wav core/wav

We also tell xmlrpc2di to export the DI APIs for monitoring and dsm, this 
way we can call the XMLRPC functions directly without the module name:
xmlrpc2di.conf:
 xmlrpc_port=8090
 export_di=yes
 direct_export=dsm;monitoring

Now we go into the core directory, and start sems (If we have not built 
sems binary yet, we enable monitoring in Makefile.defs:
Makefile.defs:
 USE_MONITORING=yes
and make SEMS and necessary applications: 
 make -C core
 make -C apps/dsm
 make -C apps/xmlrpc2di
 make -C apps/monitoring
): 

$ cd core
$ ./sems -f etc/sems.conf -D 3 -E 

we see SEMS starting up and loading the DSM script.

Now when we call into SEMS, we hear the welcome message. We will now manipulate the session from a python script: 

$ python
Python 2.5.2 (r252:60911, Oct 17 2008, 02:42:54)
[GCC 4.1.2 (Gentoo 4.1.2)] on linux2
Type "help", "copyright", "credits" or "license" for more information.
>>> from xmlrpclib import *
>>> s = ServerProxy("http://localhost:8090")
>>> s.calls()
1

calls() is a built in function that shows the active call count. 
From monitoring, we can now list the calls:

>>> s.list()
['2ED5BB64-49B7DA860003C9B1-B6E7DB90']

we can also get all log properties for that call:
>>> call_id = s.list()[0]
>>> s.get(call_id)
[{'username': '35', 'from': '<sip:1@stefan-lap.office.iptego.de>', 'app': 'dsm_di_monitoring', 'to': '<sip:35@stefan-lap.office.iptego.de>', 'position': 'lobby', 'ruri': 'sip:35@stefan-lap.office.iptego.de', 'dir': 'in'}]


We can see that the position of that call is the lobby. Now lets take that one into a room:
>>> s.postDSMEvent(call_id, [['action', 'take'],['roomname', 'wonderland']])
[200, 'OK']

We hear the 'taken to the room' message. And, if we get the session's log again:
>>> s.get(call_id)
[{'username': '35', 'from': '<sip:1@stefan-lap.office.iptego.de>', 'app': 'dsm_di_monitoring', 'to': '<sip:35@stefan-lap.office.iptego.de>', 'position': 'wonderland', 'ruri': 'sip:35@stefan-lap.office.iptego.de', 'dir': 'in'}]

We are in wonderland... We can also filter the calls by position:
>>> s.listByFilter(['position', 'lobby'])
''
>>> s.listByFilter(['position', 'wonderland'])
['2ED5BB64-49B7DA860003C9B1-B6E7DB90']

And we can take that call into another room: 
>>> s.postDSMEvent(call_id, [['action', 'take'],['roomname', 'realworld']])
[200, 'OK']

and also end it:
>>> s.postDSMEvent(call_id, [['action', 'kick']])
[200, 'OK']

which will make SEMS send a BYE to end the call.

Conclusion
----------

With the extensibility of DSM through C++ modules, tricky parts can be 
implemented as actions and conditions, and this implementation separated
from the service/application logic.

Interfacing to VoIP applications through external interfaces is simplified 
greatly through the combination of monitoring and event passing.

Converged application development could hardly be easier to write than with this 
combination of monitoring, dsm, and xmlrpc2di. Of course the real value lies in 
the application idea, and not in the technology, but hopefully this tutorial
has shown a way to quickly implement some ideas.


Further recommended reading:

 doc/Readme.monitoring
 apps/dsm/doc/Readme.dsm.txt
 apps/dsm/doc/dsm_syntax.txt
 apps/dsm/doc/Readme.dsm
 apps/dsm/mods/mod_sys/Readme.mod_sys.txt
 apps/dsm/mods/mod_uri/Readme.mod_uri.txt
 apps/dsm/mods/mod_dlg/Readme.mod_dlg.txt
 apps/dsm/mods/mod_conference/Readme.mod_conference.txt
 apps/dsm/mods/mod_monitoring/Readme.mod_monitoring.txt


(C) 2009 IPTEGO GmbH
