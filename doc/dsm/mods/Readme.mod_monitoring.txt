Actions: 

actions that work on the log entry for the call 
-----------------------------------------------

monitoring.set(string property, string value) - set one property
monitoring.log - alias to monitoring.set(...)

monitoring.add(string property, string value) - add a value to a property
monitoring.logAdd - alias to monitoring.add(...)


monitoring.logVars()
  adds all variables to log (might be a lot!)

actions that work on the global log
-----------------------------------

monitoring.setGlobal(id, name, value) - set one property to table 'id'
monitoring.addGlobal(id, name, value) - add one value to property in
                                        table 'id'
