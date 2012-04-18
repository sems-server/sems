#!/usr/bin/env python

from xmlrpclib import * 
from random import *
import time

s = ServerProxy('http://127.0.0.1:8092')

r = set()

while True:
  n = randint(400, 499)
  if n in r:
    print s.removeRegistration(n)
    r.remove(n)
  else:
    print s.createRegistration(n, str(n), str(n), '192.168.5.110')
    r.add(n)
    
  time.sleep(.1)