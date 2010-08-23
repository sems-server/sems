Actions: 
 sys.mkdir(string dirname)
 sys.mkdirRecursive(string dirname)
 sys.rename(string from, string to)
 sys.unlink(string filename)
 sys.unlinkArray(string filename, string prefix)
    Array version of unlink (prefix/filename_0 .. prefix/filename_$filename_size) 
 sys.tmpnam(string varname)

Conditions: 
 sys.file_exists(string fname)
 sys.file_not_exists(string fname)
