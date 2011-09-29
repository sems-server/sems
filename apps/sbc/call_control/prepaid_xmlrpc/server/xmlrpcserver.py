#!/usr/bin/env python

from SimpleXMLRPCServer import SimpleXMLRPCServer
import string

server = SimpleXMLRPCServer(("localhost", 8000))
server.register_introspection_functions()

class MyFuncs:
	def getCredit(self, arg):
		print "Function getCredit"
		print "     Recieved Pin: ", arg
		credit=int(arg)+10
		print "          Credits: ", credit
		return credit

	def subtractCredit(self, arg):
		#Since there is no nested values,
		# the xml can placed into a standard list
		d2 = arg[0]
		subtract = d2['amount']
		credit=1000-subtract
		print "Function subtractCredit "
		print "          Recieved Arg: ", arg[0]
		print "            methodName: ", d2['methodName']
		print "                   pin: ", d2['pin']
		print "                amount: ", d2['amount']
		print "                credit: ", credit
		return credit

server.register_instance(MyFuncs())
server.serve_forever()
