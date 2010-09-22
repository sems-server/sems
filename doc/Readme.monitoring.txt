monitoring module - in-memory AVP DB

The 'monitoring' module gets information regarding calls from the core 
and applications, and makes them available via DI methods, e.g. for 
monitoring a SEMS server via XMLRPC (using xmlrpc2di), or any other 
method that can acces DI.

Even though its primary intention is to store call related information
for monitoring purposes (what its name hints at), the monitoring module
may be used as generic in-memory store, e.g. to store information that
is later accessed by calls or other external processes, or to store
information that is to be passed into future calls.

monitoring information is explicitely pushed to monitoring module via 
DI calls (See ampi/MonitoringAPI.h for useful macros). Info is always 
accessed via primary key, usually the session's local tag. Info for 
every call is organized as attribute-value pairs (one or more values), 
value can be any type representable by AmArg (SEMS' variant type). 

A call can be marked as finished. If not done before, this is done by 
the session container when deleting a session (i.e., as the session 
garbage collector in session container only runs every few seconds, 
this can lag some seconds). Finished sessions can be listed and erased
separately, to free used memory.

Internally, the monitoring module keeps info in locked buckets of calls; 
thus lock contention can be minimized by adapting NUM_LOG_BUCKETS 
(Monitoring.h), which defaults to 16 (should be ok for most cases).

monitoring must be compile time enabled in Makefile.defs by setting 
 USE_MONITORING = yes
and the monitoring module needs to be loaded.

In monitoring.conf the option can be set to run a garbage collector thread.
This will remove all info about finished sessions preiodically.

DI API
------
functions to write values, e.g. from inside SEMS (but may also be like memcache from outside):

 set(ID, key, value [, key, value [, key, value [...]]])  - set one or multiple AVPs
 add(ID, key, value)      - add a value to an AVP

 log(ID, key, value [, key, value [, key, value [...]]])  - alias to set(...)
 logAdd(ID, key, value)  - alias to add(...)

 markFinished(ID) - mark call as finished
 setExpiration(ID, time) - set expiration of item identified with ID to time 
                           in seconds since the Epoch (like time(2))

functions to get values, e.g. from the outside:
 list()            - list IDs of calls
 listByFilter(exp, exp, exp, ...) - list IDs of calls that match the filter expressions: 
                      exp of the form array of attr_name-value, 
                      e.g. listByFilter(['dir', 'in'], ['codec_name', 'GSM'])                      
 listByRegex(attr_name, regexp) - list IDs of calls that match the regular expression
                     on the attribute (string attributes only), e.g. 
                     listByRegex('r_uri', '.*mydomain.net.*')
 listActive()      - list IDs of active (unfinished) calls
 listFinished()    - list IDs of finished calls
 get(ID)           - get info for a specific call, parameter is the call ID
 getAttribute(attr_name) 
                   - get a specific attribute from all calls, parameter is the attribute name
 getAttributeActive(attr_name) 
                   - get a specific attribute from all active calls, parameter is the attribute name
 getAttributeFinished(attr_name) 
                   - get a specific attribute from all finished calls, parameter is the attribute name
 erase(ID)         - erase info of a specific call, parameter is the call ID (+free used memory)
 eraseByFilter(exp, exp, exp, ...) - list IDs of calls that match the filter expressions and erase them; filter expressions like listByFilter 
 clear()           - erase info of all calls (+free used memory)
 clearFinished()   - erase info of all finished calls (+free used memory)

(of course, log()/logAdd() functions can also be accessed via e.g. XMLRPC.)

Performance
-----------
monitoring is not very much optimized for speed. Thus, especially by 
using DI/AmArg, a lot of string comparisions and copying is performed. 
If you measure any performance figures in real life usage comparing use 
of monitoring vs. monitoring not enabled, please contribute 
(mailto:semsdev@iptel.org, http://tracker.iptel.org) to be included in 
this documentation.


TODO
----
 o codec info
 o more app specific info
 o b2bua specific info
