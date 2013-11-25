
The DonkeySM - state machines as SEMS applications

DonkeySM is a state machine interpreter for SEMS. Application 
or service logic can comfortably and accurately be defined
as state machine, in a simple textual state machine definition 
language, and executed by the dsm module as application in SEMS.

A DSM consists of states and transitions between the states.
One state is the initial state with which the DSM is started. 
The transitions have conditions. If something happens, 
i.e. the session processes an event, the transitions are checked 
in order. If all conditions match, the transition is executed. 
Transitions can contain actions, which are run when the transition 
is executed. States can have actions that are executed on entering 
the state, and on leaving the state. If a transition is executed, 
first the leaving actions of the old state, then the transition 
actions, and finally the entering actions of the new state are 
executed.

DSMs can be defined in a hierarchical manner: Another DSM can be 
called as sub-DSM, or one can jump to another DSM, ignoring where
we came from.

In the DSM language, there is support for 
 - functions (groups of action commands)
 - if condition { action; action; } else { action; action; }
 - for loops: 
     - for ($x in range(0, 5)) { action; action; }
     - for ($x in $myarray) { action; action; }
     - for ($k,v in $mystruct) { action; action; }

A session (call) in the DonkeySM has a set of named (string) variables.
The variables may be used as parameter to most conditions and 
actions, by prepending the variable name with a dollar sign. The
parameters of an event (e.g. the key on key press) may be accessed
by prepending the name with a hash (e.g. #key). There are also 
'selects' with which a set of dialog properties can be accessed 
(e.g. @local_tag).

The DonkeySM can be extended by modules, which add new conditions
and actions to the language. This way, menuing system etc can be 
implemented as DSM, while complex logic or processing can efficitently 
be implemented in C++. Modules can act on new sessions, and have a 
initialization function that is called when the module is loaded.
DonkeySM also has built in actions to call 
DI methods from other modules. 

Actions (and conditions) can throw exceptions. Once an exception occurs,
execution of the current actions is interrupted. Exceptions are handled
this way that special "exception" transitions are executed. Exception
transitions are marked with "exception" in the conditions list. Once the
FSM is in exception handling, only exception transitions are followed.
DSMs may throw exceptions with the throw(<type>) action or the 
throwOnError() action.

DSM can cache a set of prompts, configured at start, in memory 
using PromptCollection.

A patch for fmsc 1.0.4 from the graphical FSM editor fsme 
(http://fsme.sf.net) is available, so DSMs can be defined in 
click-n-drag fashion and compiled to SEMS DSM diagrams.

DSM scripts can include other scripts by using the #include "script.dsm"
directive. That loads a script from the load path (where the current 
script resides), unless an absolute path is given (e.g. 
#include "/path/to/script).

There is SIP Session Timer (RFC4028) support, which is configured in 
dsm.conf. By default, session timers are turned not enabled.

SystemDSMs
==========

A system DSM is executed without a corresponding call. This can be useful 
e.g. to execute something periodically, to make a call generator etc.

Another use of system DSMs is to centralize application logic that spans
several calls. The call legs send updates in theirs states as events to
the central system DSM, which centrally processes those events and sends
commands as events back to the call legs, which then process those commands.

Obviously, only limited functionality is available in System DSMs, all 
call and media related functionality is not available (and will throw 
exceptions with type 'core').

A system DSM receives the "startup" event on start of the server, or if 
it is created via createSystemDSM DI call. It gets a "reload" event if the
system DSM is created by a live config reload.

On server shutdown, a system DSM receives a "system" event with 
"ServerShutdown" as type. 

See test_system_event.dsm example for an example how to handle server
start and reload.

DI commands
===========

DI commands allow interaction with DSM calls, and DSM script reload:

postDSMEvent(string call_id, [ [[param0,val0],[param1,val1],...] ]
 post a DSM event into a call. can be used to interact with running
 calls in DSM. See DSM + monitoring + DI example in 
 examples/dsm_di_monit. 

 Example: 
   s.postDSMEvent(call_id, [['action', 'take'],['roomname', 'realworld']])


reloadDSMs()
  reload all DSMs from config file (load_diags) from main config
  - DSM is loaded with main config

loadDSM(string diag_name)
  load DSM with name diag_name, paths are taken from config file
  - DSM is loaded with main config

loadDSMWithPaths(string diag_name, string diag_path, string mod_path)
  load DSM with specified paths
  - DSM is loaded with main config

preloadModules()
  preload all modules specified in config file (preload_mods)

preloadModule(string mod_name, string mod_path)
  preload module from specific path 

hasDSM(string diag_name, [string config])
  returns 1 if DSM with diag_name is loaded, 0 if not
  if config empty or not given, DSM of main config will be listed

listDSMs([string config])
  return list of loaded DSMs
  if config empty or not given, DSM of main config will be listed

registerApplication(string diag_name, [string config])
  register DSM with name diag_name as application in SEMS
  (e.g. to be used with application=$(apphdr), $(ruri.param) 
  or $(ruri.user)
  if config empty or not given, DSM is assumed to be in main config

loadConfig(string conf_file_name, string conf_name)
  (re)load application bundle ("app bundle"), configuration and scripts
  like a file in conf_dir

createSystemDSM(string conf_name, string start_diag)
  run a system DSM (i.e. a DSM thread not connected to a session)
  using scripts/configuration from conf_name. 
  conf_name=='main' for main scripts/main config (from dsm.conf)

More info
=========
 o doc/dsm_syntax.txt has a quick reference for dsm syntax
 o doc/examples/ and lib/ some example DSMs
 o doc/examples/dsm_di_monit example on interfacing with DSM
 o mods/ (will) have modules

Internals
=========
The DSMStateEngine has a set of DSM diagrams which are loaded by 
the DSMStateDiagramCollection from text file and interpreted by
the DSMChartReader, a simple stack based tokenizing compiler.

DSMCall, which implement the DSMSession interface (additionally
to being an AmSession), run DSMStateEngine::runEvent for every event 
that occurs that should be processed by the engine (e.g. Audio event, 
onBye, ...). 

The DSMStateEngine checks every condition of the active state whether
it matches. If all match, the exit actions of the current state, the 
transition actions and then the enter actions of the next state are 
executed. The DSMCondition::match and DSMAction::execute functions
get the event parameters and the session as parameters, so that they
can operate on variables, implement selects etc. 

The DSMCall implementation is very simple, it uses a playlist and 
has PromptCollection to simply play prompts etc.

DSMCoreModule is a 'built in' module that implements the basic 
conditions (test(), hangup() etc) and actions (set(), playFile(), DI 
etc).

Roadmap
=======

On the roadmap is the possibility for modules to be the session factory
used for creating the new session. As the DSMSession is mostly an abstract 
interface, other session types can easily be implemented, and their 
functionality be exposed to the DSM interpreter by custom actions and 
conditions that interact with that specific session type.

As the call state representation is nicely encapsulated here, this can 
also provide an abstraction layer on which active call replication can 
be implemented (rather than re-doing that for every application).

Q&A
===

Why "Donkey" SM? 
 Thanks Atle for the name: "I dont know why.. but my first thought
 when I read good name for FSM interpreter was Donkey. The reason for the
 name is that you put alot of things ontop of a donkey, and let it carry
 it arround.. and this is the same.. you put loads of stuf ontop.. and it
 carries it."

What is repost()? 
 If an event should be reevaluated, e.g. by the transitions of another 
 state or the initial state of another DSM which is called with callFSM/ 
 jumpFSM, repost() can be called, which signals the interpreter to
 evaluate the current event again.

Is the diagram interpreted or compiled? 
 DonkeySM reads the DSM script and creates an internal representation 
 (DSM classes in STL containers). This way it can be executed very
 efficiently.

map<string, string> var; - Are you crazy? E stands for Express!
 yes, right, there would be more efficient ways to implement
 that.  Anyway, in my experience one mostly has to manipulate and 
 check strings, and it is just very comfortable to do it this way.
 OTOH, if in a normal call there is a transition maybe on average 
 every 10 seconds, for which 5 conditions are checked, it is not 
 so much an impact on performance, considering that we are processing
 an audio packet every 20 ms.

You rely too heavily on polymorphism and RTTI - one dynamic_cast for 
each condition of each transition is way too heavy!
 Sure, but as noted above, there should not be heavy processing done
 in the DSM. If you need this, then consider writing your app entirely 
 in C++.

SEMS has a dynamically typed type (AmArg), why not use that one for 
variables? That would also make DI simpler. 
 a patch is very welcome, best to semsdev list: semsdev@iptel.org or 
 the tracker: http://tracker.iptel.org.
 There is also the avar array ("AmArg-Var"), which can hold AmArg 
 variables.

some performance numbers? 
 unfortunately not yet for running DSMs. DSM processing is actually fast: 
 a (quite simple) 3 state 5 transition DSM compiles in 0.2ms on P-M 2GHz.
 So the diagram could actually be read when the call is setup, or DSMs could
 load other DSMs (e.g. loadFSM() action)
