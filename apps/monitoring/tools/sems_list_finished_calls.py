#!/usr/bin/python3
from xmlrpc.client import ServerProxy

s = ServerProxy("http://localhost:8090")
print(f"Active calls: {s.calls()}")
print(s.di("monitoring", "listFinished"))
