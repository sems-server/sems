
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

A session (call) in the DonkeySM has a set of named (string) variables.
The variables may be used as parameter to most conditions and 
actions, by prepending the variable name with a dollar sign. The
parameters of an event (e.g. the key on key press) may be accessed
by prepending the name with a hash. There are also 'selects' with 
which a set of dialog properties can be accessed (e.g. @local_tag).

The DonkeySM can be extended by modules, which add new conditions
and actions to the language. This way, menuing system etc can be 
implemented as DSM, while complex logic or processing can efficitently 
be implemented in C++. Modules can act on new sessions, and have a 
initialization function that is called when the module is loaded.
DonkeySM also has built in actions to call 
DI methods from other modules. 

It can cache a set of prompts, configured at start, in memory 
using PromptCollection.

A patch for fmsc 1.0.4 from the graphical FSM editor fsme 
(http://fsme.sf.net) is available, so DSMs can be defined in 
click-n-drag fashion and compiled to SEMS DSM diagrams.

More info
=========
 o doc/dsm_syntax.txt has a quick reference for dsm syntax
 o doc/examples/ and lib/ some example DSMs
 o mods/ (will) have modules

Internals
=========
The DSMStateEngine has a set of DSM diagrams which are loaded by 
the DSMStateDiagramCollection from text file and interpreted by
the DSMChartReader, a simple stack based tokenizing compiler.

DSMDialogs, which implement the DSMSession interface (additionally
to being an AmSession), run DSMStateEngine::runEvent for every event 
that occurs that should be processed by the engine (e.g. Audio event, 
onBye, ...). 

The DSMStateEngine checks every condition of the active state whether
it matches. If all match, the exit actions of the current state, the 
transition actions and then the enter actions of the next state are 
executed. The DSMCondition::match and DSMAction::execute functions
get the event parameters and the session as parameters, so that they
can operate on variables, implement selects etc. 

The DSMDialog implementation is very simple, it uses a playlist and 
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

Another direction is  python/lua/... interpreter module, so that 
conditions and actions can be expressed in a more powerful language.

A set of modules exposing more of the core functionality.

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
 the tracker: http://tracker.iptel.org

some performance numbers? 
 unfortunately not yet for running DSMs. DSM processing is actually fast: 
 a (quite simple) 3 state 5 transition DSM compiles in 0.2ms on P-M 2GHz.
 So the diagram could actually be read when the call is setup, or DSMs could
 load other DSMs (e.g. loadFSM() action)
