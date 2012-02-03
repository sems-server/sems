
REDIS blacklist call control module

 This call control module can check a REDIS (http://redis.io) DB for a blacklist.

 If the queried value is found int he blacklist, the call is refused
 with "403 Unauthorized" or dropped.

 The query to execute at REDIS can be configured freely, by setting argc and argv.
 Any non-zero value/non-empty string value returned will be evaluated as blacklist
 hit.

Requirements:
  hiredis - C REDIS client library, from github.com/antirez/hiredis.git ,
            e.g. $ git clone git://github.com/antirez/hiredis.git

Parameters:

argc      - number of arguments
argv_<no> - command and arguments
action    - "drop" : silently drop
            "refuse" (default) refuse with 403 Unauthorized

Example:

Check SET 'blacklist' for the From URI user part, refuse if found:

 call_control=bl_redis
 bl_redis_module=cc_bl_redis
 bl_redis_argc=3
 bl_redis_argv_0=SISMEMBER
 bl_redis_argv_1=blacklist
 bl_redis_argv_2=$fU


Check SET 'blacklist' for the Request URI user part, drop if found:

 call_control=bl_redis
 bl_redis_module=cc_bl_redis
 bl_redis_action=drop
 bl_redis_argc=3
 bl_redis_argv_0=SISMEMBER
 bl_redis_argv_1=blacklist
 bl_redis_argv_2=$rU
