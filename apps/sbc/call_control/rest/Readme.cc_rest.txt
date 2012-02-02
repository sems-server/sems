REST call control module
========================

This call control module asks HTTP server for replacements of call profile data
and uses them when processing incoming invite ("start" DI function).

Data are retrieved from URL which is given as call control module parameter
"url". They are expected in simple text file containing parameters which should
be replaced formatted like:

    parameter 1 name = parameter 1 value
    parameter 2 name = parameter 2 value
    ...

Currently are supported following parameters:

Module parameters
-----------------

url

    URL from which is data retrieved, usual replacements are done as with other
    call control module parameters.


Example call profile
--------------------

call_control=rest
rest_module=cc_rest
rest_url=http://127.0.0.1/~kubartv/$fU/$rU

Example data file
-----------------

ruri = sip:1001@vku-test.com
from = sip:fero@vku-test.com
next_hop_ip = 192.168.1.202
next_hop_port = 5062

