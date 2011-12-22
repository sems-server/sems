CURL module

(C) 2009-2010 TelTech Systems
(C) 2011 Stefan Sayer

mod_curl can be used to retrieve web pages via http/https, use RESTful APIs etc.

Note: running curl functions is blocking - be prepared for some latency in processing
if threadpool is enabled.

Dependencies: libCURL - libcurl-dev (http://curl.haxx.se/)

Control Variables
-----------------
 $curl.timeout     timeout in seconds, e.g. 5 set($curl.timeout=5)

 $curl.out         result of get and postGetResult

 $curl.err         en error: more verbose error message

libcurl functions 
-----------------
On error, errno is set, and $curl.err contains a 
more verbose error message.

curl.get(string url)
  -- output in $curl.out

curl.getDiscardResult(string url)
  -- output is print only to debug log 

curl.getFile(string url, string output_file)
  -- gets output into output_file (mode wb)

curl.getForm(string url, string params_list)
  -- params_list is a semicolon-separated list of 
  -- variables that are passed to the form as get
  -- example :
  --   curl.getForm(http://myappserver.net/example.cgi, $id;$method;$username)

curl.post(string url, string params_list)
  -- params_list is a semicolon-separated list of 
  -- variables that are passed to the form as POST parameters
  -- example :
  --   curl.post(http://www.google.de/webhp, $q)
  --  
  -- output in $curl.out

curl.postDiscardResult(string url, string params_list)
  -- params_list is a semicolon-separated list of 
  -- variables that are passed to the form as POST parameters
  -- example :
  --   curl.postDiscardResult(http://myappserver.net/example.cgi, $id;$method;$username)

