Actions: 
 sys.mkdir(string dirname)
 sys.mkdirRecursive(string dirname)
 sys.rename(string from, string to)
 sys.unlink(string filename)
 sys.unlinkArray(string filename, string prefix)
    Array version of unlink (prefix/filename_0 .. prefix/filename_$filename_size) 
 sys.tmpnam(string varname)

 sys.popen($var="command")
   execute a command (using popen) and save result in $var
   example:
     sys.popen($myfiles="/bin/ls wav/*");
   throws exceptions
    #type=="popen", #cause==reason if fails to exec
    #type=="pclose", #cause==reason if fails to close pipe
 
 sys.getTimestamp(string varname) - get timestamp in varname.tv_sec/varname.tv_usec
 sys.subTimestamp(ts1, ts2)  - subtract $ts2 from $ts1, save result in
                               $ts1.sec, $ts1.msec and $ts1.usec

Conditions: 
 sys.file_exists(string fname)
 sys.file_not_exists(string fname)


