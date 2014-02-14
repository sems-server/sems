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

Following parameters are supported now: 

    ruri, from, to, contact, call-id, outbound_proxy, force_outbound_proxy,
    next_hop_ip, next_hop_port, next_hop_for_replies, 

    append_headers, 
    header_filter, header_list

    sst_enabled

    rtprelay_interface, aleg_rtprelay_interface,

    outbound_interface

These are unsupported (see todo):

    refuse_with, sst_aleg_enabled,


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

TODO
----
 - configurable data format:
   - json
   - XML
   - text
 - test
 - support for other call profile parameters
 - changing some call profile parameters doesn't take effect because they are
   evaluated before call control modules are called ... might be fixed by calling
   the call control modules at the very beginning of processing
   (SBCFactory::onInvite)
