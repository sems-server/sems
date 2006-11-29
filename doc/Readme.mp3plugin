Lame MP3 file writing plug-in for SEMS Readme

Description
-----------
This mp3 plug-in enables MP3 file writing for SEMS. In order to use
it, you must get the lame library sources first. Files recorded with
the extension "MP3" or "mp3" are recorded as mp3 if possible. 

Quickstart
----------
* install lame (e.g. emerge lame) 
* cd apps/mp3 ; make

Installation
------------
1) Get lame from lame.sourceforge.net (
http://sourceforge.net/project/showfiles.php?group_id=290)
2) Unpack the archive; make ; make install 
3) make in plug-in/mp3 directory

If it complains about not being able to find lame.h, 
edit LAME_DIR in mp3 plug-in Makefile to point to the location of
the unpacked lame source archive and re-make

By default, the mp3 module is not built (as you need to get lame first).
If you want to build the mp3 module by default as well, remove mp3 
from exclude_modules in apps/Makefile 

Important Note:
--------------
Using the LAME encoding engine (or other mp3 encoding technology) in
your software may require a patent license in some countries.
See http://www.mp3licensing.com/ or
http://lame.sourceforge.net/links.html#patents for further information.




