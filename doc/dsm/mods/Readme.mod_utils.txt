Actions
-------

 utils.getNewId(string varname)

 utils.playCountRight(int cnt [, string basedir])
    play count for laguages that have single digits after the 10s (like english)
    * Throws "file" exeption with #path if file can not be opened
    * sets $errno (arg)

 utils.playCountLeft(int cnt [, string basedir])
    play count for laguages that have single digits before the 10s (like german)  
    * Throws "file" exeption with #path if file can not be opened
    * sets $errno (arg)

 utils.getCountRight(int cnt [, string basedir])
    get filenames for a number for laguages that have single digits after the 10s (like english)
    into count_file[n]  (i.e. count_file[0] .. count_file[n])
    * sets $errno (arg)

 utils.getCountLeft(int cnt [, string basedir])
    get filenames for a number for laguages that have single digits before the 10s (like german)  
    into count_file[n]  (i.e. count_file[0] .. count_file[n])
    * sets $errno (arg)

 utils.getCountRightNoSuffix(int cnt [, string basedir])
 utils.getCountLeftNoSuffix(int cnt [, string basedir])
    as above but without .wav suffix

 utils.spell(string word[, string basedir])
  plays each character in the word (e.g. utils.spell(321,wav/digits/) plays
    wav/digits/3.wav, wav/digits/2.wav, wav/digits/1.wav 
  (like SayDigits from *)
  * Throws "file" exeption with #path if file can not be opened

 utils.rand(string varname [, int modulo])
  generates random number: $varname=rand()%modulo or $varname = rand()

 utils.srand() 
  seed the RNG with time().

 utils.add($var, val)
   add val (float value) to var. also 
   utils.add($var1, $val); utils.add($var, #param)

 utils.sub($var, val)
   subtract val from var

 utils.mul($var, val)
   multiply integer valued var by integer value val

 utils.int($var, val)
   get integer part of val into var

 utils.md5($var, val)
   calculate md5 hex digest of val to $var

 utils.replace($subject, (search|$search)=>(replace|$replace))
   in $subject, replace each search string with replace string   

 utils.splitStringCR($var [, $dstvar])
 utils.splitStringCR(val, $dstvar])
   split a string on newline (carriage return, \n) 
   into an array ($var[0]..$var[n])

   example: 
    sys.popen($myresult="/bin/ls wav/*");    
    utils.splitStringCR($myresult);

 utils.splitString($var, delim|$delim)
   split string in $var on delim or $delim into array $var[0], ...
   if delim is empty, splits string on every character

 utils.decodeJson($var, $prefix)
   decode jSON document string in $var into $prefix struct or array
   
 utils.escapeCRLF($var)
   replace CRLF (\r\n) in string with escaped CRLF (\\r\\n) 

 utils.unescapeCRLF($var)
   replace escaped CRLF (\\r\\n) in string with CRLF (\r\n) 


 utils.playRingTone(length [, on [, off [, f [, f2]]]])
   play a RingTone (ringback tone)
   defaults to length=0 (indefinite), on 1000ms, off 2000ms, f 440Hz, f2 480Hz

   Example:
      utils.playRingTone(0, 1000, 4000, 425, 0);   -- Germany

Conditions
----------

 utils.isInList(key, cs_list) - match if key is in comma-separated list
   e.g. if utils.isInList(#sip_code, "404, 405") { ... }

 utils.startsWith(subject|$subject, prefix|$prefix) - match if subject
   starts with prefix
