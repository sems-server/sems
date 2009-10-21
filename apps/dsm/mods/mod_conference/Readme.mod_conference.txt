Actions: 
 conference.join(string roomname [, string mode])
   mode = "" | speakonly | listenonly

 conference.leave()
   destroy conference channel. Close playlist first!!!!!

 conference.rejoin(string roomname [, string mode])
   mode = "" | speakonly | listenonly

 conference.postEvent(string roomname, int event_id)

 conference.setPlayoutType(string type)
   where type is one of ["adaptive", "jb", "simple"]
