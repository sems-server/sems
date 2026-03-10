monitoring module - in-memory AVP DB

The 'monitoring' module gets information regarding calls from the core
and applications, and makes them available via DI methods, e.g. for
monitoring a SEMS server via XMLRPC (using xmlrpc2di), Prometheus
(using sems-prometheus-exporter), or any other method that can access DI.

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

monitoring is enabled by default. To disable it, build with:
 cmake .. -DSEMS_USE_MONITORING=no
The monitoring module needs to be loaded in sems.conf.

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

Counters and Samples
--------------------
In addition to per-call attribute-value pairs, the monitoring module supports
named counters and time-series samples:

 inc(type, name)             - increment a named counter
 dec(type, name)             - decrement a named counter
 addCount(type, name, value) - add a value to a named counter
 addSample(type, name [, count [, timestamp]])
                             - record a timestamped sample for rate calculation
 getCount(type, name [, seconds])
                             - get counter/sample count within a time window
 getAllCounts(type [, seconds])
                             - get all counters/samples for a type
 getSingle(ID, key)          - get a single attribute value

Samples are retained for a configurable window (retain_samples_s in
monitoring.conf, default 10 seconds) and can be used for rate calculations.

These counters are accessible via DI, XMLRPC, and the Prometheus exporter.

Command-line Tools
------------------
Rust-based CLI tools are provided for querying the monitoring module via
XMLRPC (requires xmlrpc2di to be running). Python fallback scripts are
installed when a Rust toolchain is not available.

 sems-list-calls [--full] [--url <url>]
   List all monitored call IDs. With --full, print detailed properties
   for each call.

 sems-list-active-calls [--full] [--url <url>]
   List only active (unfinished) call IDs.

 sems-list-finished-calls [--url <url>]
   List finished call IDs awaiting garbage collection.

 sems-get-callproperties [--url <url>] <call-id>
   Get all attributes for a specific call by its ID (local tag).

Default XMLRPC URL is http://localhost:8090. Use --url to override.

Prometheus Exporter
-------------------
sems-prometheus-exporter is a standalone HTTP server that scrapes SEMS
via XMLRPC and serves metrics in Prometheus exposition format.

Usage:
 sems-prometheus-exporter [--url <sems-xmlrpc-url>] [--listen <addr:port>]

   --url     SEMS XMLRPC endpoint (default: http://localhost:8090)
   --listen  Prometheus HTTP listen address (default: 0.0.0.0:9090)

Requires the xmlrpc2di module to be loaded and running in SEMS, with
export_di=yes in xmlrpc2di.conf.

Metrics exposed on /metrics:

  Core metrics (from built-in XMLRPC methods):
    sems_active_calls          - current number of active calls (gauge)
    sems_sessions_total        - total sessions since startup (counter)
    sems_calls_avg             - average active calls, 5s window (gauge)
    sems_calls_max             - peak active calls since last query (gauge)
    sems_cps_avg               - average calls per second, 5s window (gauge)
    sems_cps_max               - peak CPS since last query (gauge)
    sems_shutdown_mode         - whether shutdown mode is active (gauge)
    sems_cps_hard_limit        - configured hard CPS limit (gauge)
    sems_cps_soft_limit        - configured soft CPS limit (gauge)

  Monitoring plugin metrics (via DI):
    sems_monitoring_count{name="..."} - named counter values (gauge)
    sems_monitoring_active_sessions   - active monitored sessions (gauge)
    sems_monitoring_finished_sessions - finished sessions awaiting GC (gauge)

  Registration metrics (if present in monitoring attributes):
    sems_reg_active            - active registrations (gauge)
    sems_registrations         - total registrations (gauge)
    sems_registered_uas        - registered user agents (gauge)

Custom application counters set via MONITORING_INC/MONITORING_DEC macros
or monitoring.inc/monitoring.dec DSM actions are automatically exported
as sems_monitoring_count{name="..."} labels.

Performance
-----------
monitoring is not very much optimized for speed. Thus, especially by
using DI/AmArg, a lot of string comparisions and copying is performed.
If you measure any performance figures in real life usage comparing use
of monitoring vs. monitoring not enabled, please contribute
(mailto:semsdev@iptel.org, http://tracker.iptel.org) to be included in
this documentation.

The Prometheus exporter connects to SEMS via XMLRPC on each scrape,
so scrape intervals below 5 seconds are not recommended for high-traffic
systems.

TODO
----
 o codec info
 o more app specific info
 o b2bua specific info
