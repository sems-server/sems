#!/usr/bin/env python
from xmlrpclib import * 
s = ServerProxy('http://127.0.0.1:8092')
print s.setTarget(100, 1, 0, '4', '192.168.5.110', 2, 0, 30, 40)

