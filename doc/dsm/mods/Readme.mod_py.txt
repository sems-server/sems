mod_py - DSM actions and conditions in Python
=============================================

mod_py adds one action and one condition to DSM: py(...)

py() action may execute arbitrary python code; py() condition 
python code needs to evaluate to an expression which returns
True or False (or and int value).

The 'type' and 'params' are accessible to determine the current
event's type and parameters.

py() actions and conditions can access session's variables using
session.var(name) and session.setvar(name, value). Locals stay 
across different py(...) actions/conditions.

They may even directly use some media functionality implemented 
in DSM sessions - see session module's help below. But, 
conceptionally, mod_py is above DSM, so while it would be 
possible it is not recommended to extend it with functions that
manipulate the AmSession directly.

Indentation is a little ugly with multi-line py() actions. 
But this is Python's fault (in 21st century, who creates a 
programming language with fixed indentation?).

mod_py MUST be preloaded, to initialize the python interpreter.
add preload=mod_py to dsm.conf. 


locals
======
type    - event type (dsm.Timer, dsm.Key, ...)
params  - dictionary with event parameters 
           (e.g. params['id'] for event==dsm.Timer)
dsm     - module to access dsm functions (see below)
session - module to access session functions (see below)

exceptions
==========
dsm.playPrompt, dsm.playFile, dsm.recordFile and dsm.setPromptSet
may throw an exception. This is a Python RuntimeError exception,
not the DSM exception handling (exceptions in interpreted code
can not be catched through outside of the interpreter). 
The application needs to catch the exceptions in the python code
itself, and can then handle them or signal the DSM code to throw
an exception. example:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
initial state begin 
 enter {
  py(dsm.INFO("hello dsm-world\n"))
  py(#
try:
	session.playFile('doesnotexist.wav')
except RuntimeError, e:
	dsm.ERROR('thats a runtime error: ' + e.message + '!\n')
	session.setvar('exception', e.message)
)
  log(2, huhu, still there);
  repost();
};
transition "catch py exception" begin - test($exception!="") / throw($exception) -> begin;
transition "normal exception handling" begin - exception; test(#type=="file") -> exception_state;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


example
=======
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
import(mod_py);

initial state begin 
 enter {
  py(dsm.INFO("hello dsm-world"))
  py(#
session.setvar('some_variable','some val')
print session.var('some_variable')
print "dsm.Timer = ", dsm.Timer
)
  setTimer(1, 5);
  log(2, $some_variable)
  repost();
};

transition "timer" begin - py(type == dsm.Timer and params['id'] == '1')  / 
         py(session.playFile('wav/default_en.wav')); -> wait;

transition "key 1" begin - py(type == dsm.Key and params['key'] == '1')  / 
         py(session.playFile('wav/default_en.wav')); -> wait;

state wait;
transition "bye recvd" (begin, wait) - hangup / stop -> end;
state end;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

modules
=======
------------------------
help on module session:

NAME
    session

FILE
    (built-in)

FUNCTIONS
    B2BconnectCallee(string remote_party, string remote_uri [, bool relayed_invite])
        connect callee of B2B leg

    B2BterminateOtherLeg()
        terminate other leg of B2B call

    addSeparator(string name[, bool front])
        add a named separator to playlist

    flushPlaylist()
        close the playlist

    connectMedia()
        connect media (RTP processing)

    disconnectMedia()
        disconnect media (RTP processing)

    getRecordDataSize()
        get the data size of the current recording

    getRecordLength()
        get the length of the current recording

    mute()
        mute RTP)

    playFile(string name [, bool loop[, bool front]])
        play a file

    playPrompt(string name [, bool loop])
        play a prompt

    recordFile(string name)
        start recording to a file

    select(string name)
        get a session's select

    setError(int errno)
        set error (errno)

    setPromptSet(string name)
        set prompt set

    setvar(string name, string value)
        set a session's variable

    stopRecord()
        stop the running recording

    unmute()
        unmute RTP

    var(string name)
        get a session's variable
------------------------------------
Help on module dsm:

NAME
    dsm

FILE
    (built-in)

FUNCTIONS
    DBG(string msg)
        Log a message using SEMS' logging system, level debug

    ERROR(string msg)
        Log a message using SEMS' logging system, level error

    INFO(string msg)
        Log a message using SEMS' logging system, level info

    WARN(string msg)
        Log a message using SEMS' logging system, level warning

    log(int level, string msg)
        Log a message using SEMS' logging system

DATA
    Any = 0
    B2BOtherBye = 13
    B2BOtherReply = 12
    DSMEvent = 10
    Hangup = 6
    Hold = 7
    Invite = 1
    Key = 3
    NoAudio = 5
    PlaylistSeparator = 11
    SessionStart = 2
    Timer = 4
    UnHold = 8
    XmlrpcResponse = 9



how to debug memory leak:
-------------------------

Unfortunately, it seems to be not simple to get embedded 
python interpreter leak free. Here is how to run with python's 
mem debug:

compile python with --with-pydebug, e.g. 

./configure --with-pydebug --prefix=/path/to/mod_dsm/python
make && make install

set debug python in Makefile, e.g. replace PY_VER/PY_EXE:
 PY_VER = 2.5
 PY_EXE = ./python/bin/python

make mod_py with -D PYDSM_WITH_MEM_DEBUG

run sems with -E, make calls, end sems with ctrl-c
from python do scripts/combinerefs.py refs.txt.
generate calls with e.g. sipp:
sipp -sn uac -i 192.168.5.106 -s 35 -d 500 -r 400 192.168.5.106:5070
(sudo su; ulimit -n 100000  before starting sems)
