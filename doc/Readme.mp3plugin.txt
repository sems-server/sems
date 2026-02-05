Lame MP3 file writing plug-in for SEMS Readme

Description
-----------
This mp3 plug-in enables MP3 file writing for SEMS. In order to use
it, you must have the lame library installed. Files recorded with
the extension "MP3" or "mp3" are recorded as mp3 if possible.

Building with MP3 support
-------------------------
1) Install lame and mpg123 development packages:
   - Debian/Ubuntu: apt-get install libmp3lame-dev libmpg123-dev
   - RHEL/CentOS: yum install lame-devel libmpg123-devel
   - From source: https://lame.sourceforge.io/

2) Configure SEMS with MP3 support enabled:
   mkdir -p build && cd build
   cmake .. -DSEMS_USE_MP3=yes
   make

Important Note:
--------------
Using the LAME encoding engine (or other mp3 encoding technology) in
your software may require a patent license in some countries.
See http://www.mp3licensing.com/ or
http://lame.sourceforge.net/links.html#patents for further information.
