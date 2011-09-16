syslog CDR generation (e.g. for SBC)
------------------------------------

This module implements the "cdr" DI interface to generate CDRs and write them in CSV
format to syslog.

The CDR is stored in memory until the call is ended.


The CSV format is:
 A leg local tag,Call-ID,From Tag,To Tag,start TS,connect TS,end TS, (...)
 
Where (...) is all further values as configured in the profile (cdr_*) in alphabetical
order.


