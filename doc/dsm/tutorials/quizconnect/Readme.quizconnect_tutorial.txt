quizconnect DSM tutorial (c) 2010 Stefan Sayer

This DSM tutorial implements step by step an application,
which is best described by the original question:

"I am trying to accomplish a script that parallel forks to 
 many callers and then the one who enters the right DTMF 
 code gets the call."

The tutorial shows how a full application can be implemented
with DSM state machine scripting alone. Specifically, it shows
how to 
 - play ringback tone (183 early media)
 - read from mysql database
 - place outgoing calls
 - interact between calls
 - use conference module to connect the audio of calls

