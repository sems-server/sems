import SimpleXMLRPCServer

#The server object
class AuthServer:
	def __init__(self):
		self.keys = {}

	def authorize(self, room, pin):
		if self.keys.has_key(room):
			if self.keys[room] == pin:
				return 'OK'
			else:
				return 'FAIL'
		else:
			self.keys[room] = pin
			return 'OK'

authsrv = AuthServer()
server = SimpleXMLRPCServer.SimpleXMLRPCServer(("127.0.0.1", 9090))
server.register_instance(authsrv)

#Go into the main listener loop
print "Listening on port 9090"
server.serve_forever()
