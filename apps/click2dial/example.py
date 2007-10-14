#!/usr/bin/python

########################################################################
# An example using python to initiate a call via di_dialer and xmlrpc2di
#
# In this case, the R-Uris of leg (a) and leg(b) are looking like
# sip:user@proxy-ip;sw_domain=domain to route the call
# via an outbound proxy "proxy". The proxy in question has to
# extract the value of sw_domain and place it into the domain
# of the R-Uri to correctly route it to the destination.
########################################################################

proxy = "192.168.100.10"
xmlrpc_url = "http://127.0.0.1:8090"

caller_user = "foo"
caller_domain = "iptel.org"
callee_user = "bar"
callee_domain = "iptel.org"

auth_user = "foo"
auth_pass = "foopass"
auth_realm = "iptel.org"

announce_file = "default_en"

from xmlrpclib import *
s = ServerProxy(xmlrpc_url)
s.dial_auth_b2b(
        "click2dial", announce_file, 
        "sip:" + caller_user + "@" + caller_domain, 
        "sip:" + callee_user + "@" + callee_domain, 
        "sip:" + caller_user + "@" + proxy + ";sw_domain=" + caller_domain,
        "sip:" + callee_user + "@" + proxy + ";sw_domain=" + callee_domain,
        auth_realm, auth_user, auth_pass)
