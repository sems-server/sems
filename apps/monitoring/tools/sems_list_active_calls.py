#!/usr/bin/python3
from xmlrpc.client import ServerProxy
import pprint
import sys

if len(sys.argv) == 2 and sys.argv[1] == "--help":
    print("usage: %s [--full]" % sys.argv[0])
    sys.exit(1)

s = ServerProxy("http://localhost:8090")
print("Active calls: %d" % s.calls())
ids = s.di("monitoring", "listActive")

pp = pprint.PrettyPrinter(indent=4)
pp.pprint(ids)

if len(sys.argv) == 2 and sys.argv[1] == "--full":
    for callid in ids:
        attrs = s.di("monitoring", "get", callid)
        print("----- %s -----" % callid)
        pp.pprint(attrs)
