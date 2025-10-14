#!/usr/bin/python3
import sys
import pprint
from xmlrpc.client import ServerProxy

if len(sys.argv) != 2:
    print("usage: %s <ltag/ID of call to list>" % sys.argv[0])
    sys.exit(1)

s = ServerProxy("http://localhost:8090")
print("Active calls: %d" % s.calls())
pp = pprint.PrettyPrinter(indent=4)
res = s.di("monitoring", "get", sys.argv[1])
pp.pprint(res)
