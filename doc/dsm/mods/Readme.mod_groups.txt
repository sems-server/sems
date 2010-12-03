mod_groups - Group event passing

 Copyright (C) 2010 Stefan Sayer
 
 Development of this module was sponsored by TelTech Systems Inc

Overview
--------

The groups module implements a group event broadcast system. DSM calls can
join and leave groups, and post events to groups. Events are passed to all
members of the group that the event is posted to. Events can, just like the 
`postEvent` function of DSM, contain either some variables, or all variables.
If a call ends, it is automatically removed from all groups it belongs to.

Actions: 

  groups.join(groupname)            -  Join a group
  groups.leave(groupname)           -  Leave a group
  groups.leaveAll()                 -  Leave all groups
  groups.postEvent(groupname, var1;var2)  -  post event to groupname with var1 and var2
  groups.postEvent(groupname, var)  -  post event to groupname with all variables


Example:
  import(mod_groups);
  initial state START
  enter {
   groups.join("yeah");
   repost();
  };

  transition "in the call" START --> IN_CALL;

  state IN_CALL;

  transition "got key" IN_CALL - key / {
	set($event="key_press");
	set($key=#key);
	set($duration=#duration);
	set($type=groups);
        logVars(1);
	groups.postEvent("yeah", event;key;duration;type);
  } -> IN_CALL;

  transition "got group event" IN_CALL - event(#type==groups) / {
	logParams();
  } -> IN_CALL;

  transition "hangup" IN_CALL - hangup / {
    -- groups.leave("yeah");
    groups.leaveAll();
    stop(false);
  } -> END;

  state END;
