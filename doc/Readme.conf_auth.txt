conf_auth Readme

conf_auth application: PIN collect, authentication and B2BUA 
                       connect with timeout

The conf_auth application is an ivr application which plays 
a welcome message to the caller, collects a PIN, verifies 
this PIN agains a XMLRPC authentication server, and if correct
 connects in back-to-back user agent mode to a uri returned
from the authentication server. The call is terminated after 
a timeout, if the authentication server did return a timeout
value. If the caller while collecting the PIN does not enter 
any digit for a number of seconds, she or he is prompted with
a hint message.

As this script combines diverse SEMS/ivr functionality (TTS, 
DTMF collect, b2bua, timer), it may well serve as a basis for 
customized services, and actually it is rather meant as a 
demonstration of these possibilities.

The authentication server here needs to serve only one method: 
AuthorizeConference. This method takes as argument the From URI, 
Request URI, and PIN, and returns either a tuple of 
['FAIL', <file_name>] where <file_name> is the file which should
be played to tell the user that the entered PIN is not correct, or 
['OK', <to>, <to_uri>, <timer_timeout>, <max_participants>] 
where 
 <to>               : To in the second call leg 
 <to_uri>           : Uri of the second call leg 
 <timer_timeout>    : Timeout of call (0 if no timer)
 <max_participants> : unused

An example python authentication server is given below.

#---------- auth_srv.py -------------------------------------
!/usr/bin/python

import sys
sys.path.insert(0, '../')
import xmlrpc
import traceback
import select
import string
import csv

PORT            = 23456
TIMEOUT         = 1.0
LOGLEVEL        = 3             # this is the default log level
TEST_NAME       = 'shilad'
TEST_PASS       = 'shilad'

PINS    = {
                'sip:uli@iptel.org' +  'sip:conf1@confserver.net' +  '1234' \
                        : ['test <sip:1@192.168.5.100>','sip:1@192.168.5.100']
}


# you may uncomment the 'setAuth()' line to use the example
# authentication function 'authenticate()' supplied below
#
def exampleServer():

        global exitFlag

        exitFlag = 0
        s = xmlrpc.server()
#       s.setAuth(authenticate)
        s.addMethods({
                'AuthorizeConference' : confAuthMethod,
                'add_pin'   : addPinMethod,
                'list_pins' : listPinsMethod
        })
        s.bindAndListen(PORT)
        while 1:
                try:
                        s.work()        # you could set a timeout if desired
                except:
                        e = sys.exc_info()
                        if e[0] in (KeyboardInterrupt, SystemExit):
                                raise e[0], e[1], e[2]
                        traceback.print_exc()
                if exitFlag:
                        break

def authenticate(uri, name, password):
        if name == TEST_NAME and password == TEST_PASS:
                return (1, 'a domain')
        else:
                return (0, 'a domain')

def confAuthMethod(serv, src, uri, method, params):
        print 'params are', params
        test_key = params[0] + params[1] + params[2]
        print 'test_key = ', test_key
        if not PINS.has_key(test_key):
                return ['FAIL', 'wav/failed.wav']
        else:
                return ['OK', PINS[test_key][0], PINS[test_key][1], 30, 5 ]

def addPinMethod(serv, src, uri, method, params):
        print 'params are', params
        add_key = params[0] +  params[1] + params[2]
        print 'add_key = ', add_key
        PINS[add_key] = [params[3], params[4]]
        return ['ok']

def listPinsMethod(serv, src, uri, method, params):
        return PINS

if __name__ == '__main__':
        exampleServer()

#---------- auth_srv.py -------------------------------------
