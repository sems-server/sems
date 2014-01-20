Actions: 
 conference.join(string roomname [, string mode])
   mode = "" | speakonly | listenonly
   Throws "conference" Exception if conference can not be joined (currently never).

 conference.leave()
   destroy conference channel. Flush playlist with flushPlaylist() first!!!!!
   * Sets $errno (script).

 conference.rejoin(string roomname [, string mode])
   mode = "" | speakonly | listenonly
   Throws "conference" Exception if conference can not be joined (currently never).

 conference.postEvent(string roomname, int event_id)
   * Sets $errno (arg).

 conference.setPlayoutType(string type)
   where type is one of ["adaptive", "jb", "simple"]

conference.teejoin(string roomname [, string avar_id])
   - speak also to conference with roomname
   - avar_id is the name in which conference channel is stored 
   - if this is called in the beginning of a call (sessionStart event, 
     or initial state enter block), call setInOutPlaylist before 
     conference.teejoin (teejoin uses input to connect to audio queue,
     which is normally set only after running sessionStart event in 
     inital state)

conference.teeleave([string avar_id])
   - leave tee conference (release conf channel)
   - resets playlist as input and output

conference.setupMixIn(float level, unsigned int seconds)
   - set up MixIn for mixing in files into output later
    notes: 
      o only do that when playlist is input/output 
          (possibly do setInOutPlaylist first)
      o in sessionStart event, set $connect_session=0, otherwise
        playlist will be set as output again, and MixIn does not 
        play, e.g.
          set($connect_session=0);
          setInOutPlaylist();
          conference.setupMixIn(0.5);

     o if you use setupMixIn multiple times, use setInOutPlaylist first!!!!!
       otherwise it could crash if old MixIn is still in the output / 
       the audio queue

conference.playMixIn(string filename)
    - mix in a file
