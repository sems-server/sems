
*******************************
* Conferencing plug-in README *
*******************************


Description:
------------

The 'conference' plug-in works very easy: 
every call having the same request URI will 
be in the same conference room.

Dialout feature:
----------------

Dialout is supported through AVT tones. Somebody
who has been 'dialed out' cannot use this functionnality
due to billing issues. If you still want that, you can 
turn it out in the code (search for 'if(dialedout)').

keys:
 - '#*' to trigger dialout. Then type in the number and finish with '*'.
 - you be first connected to the callee only
 - press '*' to connect the callee to the conference.
 - press '#' to drop the call.

Dialout is only enabled if dialout_suffix is defined (see below).

Configuration:
--------------

These are needed if you store audio files in file system:

audio_path: directory where conference audio files are kept (optional,
	    defaults to /usr/local/lib/sems/audio)

default_announce: Announcement to be played to the first participant as
		  he enters the conference room.  If default_announce
		  does not start with '/' char, default_announce is
		  appended to audio_path. (optional, defaults to
		  audio_path/default.wav)  If P-Language header or
		  Language P-App-Param header parameter exists,
		  announcement file
		  audio_path/lonely_user_msg/r-uri-host/default_<language>.wav, 
		  or audio_path/lonely_user_msg/default_<language>.wav
		  (if exists) is used instead of default_announce file.

join_sound: sound to be played to all the participants
            when a new one joins the conference. If join_sound does not
            start with '/', join_sound is appended to audio_path.
	    (optional)

drop_sound: sound to be played to all the participants
            when a participant leaves the conference.
	    If drop_sound does not start with '/', drop_sound is
            appended to audio_path. (optional)

These are needed if you store audio files in MySQL database:

mysql_host:	 host where MySQL server is running (optional, defaults
		 to 'localhost')

mysql_user:	 MySQL username (mandatory, no default)

mysql_passwd:	 MySQL password (mandatory, no default)

mysql_db:	 database where audio is stored

mysql_db must contain two tables as follows:

CREATE TABLE default_audio (
  id int(10) unsigned NOT NULL auto_increment,
  application varchar(32) NOT NULL,
  message varchar(32) NOT NULL,
  `language` char(2) NOT NULL default '',
  audio mediumblob NOT NULL,
  PRIMARY KEY  (id),
  UNIQUE KEY application (application,message,`language`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1;

CREATE TABLE domain_audio (
  id int(10) unsigned NOT NULL auto_increment,
  application varchar(32) NOT NULL,
  message varchar(32) NOT NULL,
  domain varchar(128) NOT NULL,
  `language` char(2) NOT NULL default '',
  audio mediumblob NOT NULL,
  PRIMARY KEY  (id),
  UNIQUE KEY application (application,message,domain,`language`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1;

The first one stores default audio files and the latter domain specific
audio files.  Application value must be 'conference' and message values
are 'first_participant_msg', 'join_snd', and 'drop_snd'.  Language value
is a two letter code of a language or '' if language is unknown.

When an audio file is fetched from database, it is stored in /tmp
directory.

These are independent of audio storage:

dialout_suffix: suffix to be append to the numbered entered by
                the user. Example: @iptel.org

playout_type:  adaptive_playout, adaptive_jb, or simple:
   		select playout mechanism. 
                adaptive_playout : Adaptive Playout buffer 
                  (default, recommended)
                adaptive_jb      : Adaptive Jitter buffer
                simple           : simple (fifo) playout buffer
	      See sems core documentation for an explanation of the 
              methods.

max_participants: Maximum number of participants in a conference.
		  Default = 0 (unlimited)

Adding participants with "Transfer" REFER:
------------------------------------------
 The "Transfer REFER" is a proprietary REFER call flow which transfers a 
 SIP dialog and session to another user agent ('taker'). If the transfer 
 REFER is  accepted, the one transfering the call just "forgets" the dialog 
 and associated session, while the taker can send a re-Invite, thus overtaking
 the dialog and session. For this to work, both transferer and taker must
 be behind the same record routing proxy, and the callers user agent must 
 properly support re-Invite (updating of contact, and session, as specified 
 in RFC3261).

 The transfer request sent out has two headers, which are needed by the 
 entity taking the call: 
  P-Transfer-RR  : route set of the call
  P-Transfer-NH  : next hop 

 These headers must be configured in ser.cfg to be passed to the conference 
 application, for example using the following tw_append: 

 # appends for REFER
 modparam( "tm", "tw_append",
   "refer_append:hdr[P-Transfer-NH];hdr[P-Transfer-RR]")

 ...
 
 t_write_unix("/tmp/msp_conference_sock","conference/refer_append");

 Note that while this request has the method 'REFER', it does not follow rfc3515,
 for example the Refer-To header is not used.

