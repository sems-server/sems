voicemail (voicemail2email) application 

This application plays a greeting message to the caller and records a 
voicemail. The voicemail is sent out as email via SMTP, saved in voicebox,
or both. 

In announcement mode, only the greeting message is played.

Audio Files
-----------

If try_personal_greeting=yes is set, the voicemail application tries to find 
a personal greeting message from msg_storage. Then the voicemail application
looks in various file system paths or database tables for announce and beep 
messages to be played. 

If file system is used to store audio files, the announcement file to be
played is looked for in the following order:

 <announce_path><did><uid>'.wav'
 <announce_path><did><language><default_announce>
 <announce_path><did><default_announce>
 <announce_path><language><default_announce>
 <announce_path><default_announce>

where <announce_path> and <default_announce> is set in voicemail.conf,
<domain> is the host part of the request URI, <user> the user part of 
the request URI (if not overridden by uid="..." or "usr=..."), and 
<language> is the App-Param 'Language' (see below).

If MySQL database is used to store audio files, the announcement to be
played is looked for first from user_audio table, then from domain_audio
table, and last from default_audio table.  The last two tables are
defined as described in Readme.conference.  user_audio table is defined
as follows:

CREATE TABLE user_audio (
  id int(10) unsigned NOT NULL auto_increment,
  application varchar(32) NOT NULL,
  message varchar(32) NOT NULL,
  domain varchar(128) NOT NULL,
  userid varchar(64) NOT NULL,
  audio mediumblob NOT NULL,
  PRIMARY KEY  (id),
  UNIQUE KEY application (application,message,domain,userid)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1;

Beep sound to be played is looked for only from default_audio table.

Application value is 'voicemail' and message values are 'greeting_msg'
and 'beep_snd'.

When an audio file is fetched from database, it is stored in /tmp
directory.

Mail Server
-----------

The mail server to send the email with is configured in voicemail.conf 
in smtp_server/smtp_port (use e.g. postfix).

Other Items
-----------

Further configuration items (voicemail.conf):

max_record_time - voicemail records voicemail up to the number of 
                  seconds set here

rec_file_ext - extension of recorded voicemail file. this defines the 
               format of the recorded message: default is .wav, .mp3 
	       will record a voicemail as MP3 file (the mp3 plugin needs
	       to be loaded so that mp3 encoding is possible)

App-Params
----------

These must be passed as headers with the INVITE request to SEMS, so that 
SEMS knows at least the email address to send the email to.

There is a short and a long form. The short form may be preferred to 
avoid packet fragmentation (UDP).

  long            short       content
  -----------------------------------------------------------------------
  Email-Address   eml         - mandatory (!): the email address to send the mail to
  Mode            mod         - message leave mode: 
                                  voicemail  : send email (default)
                                  box        : leave in voicebox (store in msg_storage)
                                  both       : send email and leave in voicebox
                                  ann        : just play greeting, don't record message.
  User            usr         - voicebox user 
  Sender          snd         - sender. If empty, From will be used
  Domain          dom         - domain specific announcement files/email template. 
                                defaults to RURI domain.
  Language        lng         - optional: language for announcement file/email template
  Type            typ         - optional (default: 'vm'): prompt file to play, for user recorded
                                greeting (see annrecorder app)
  UserID          uid         - optional: override user by user id (see Note below)
  DomainID        did         - optional: override domain by domain id (see Note below)
  

  Example:
    P-App-name: voicemail
    P-App-Param: Email-Address=user@maildomain.net;Language=Latvian

  or 

    P-App-name: voicemail
    P-App-Param: eml=user@maildomain.net;mod=both;lng=english;snd=someone@iptel.org;dom=iptel.org;

Using UserID (UID) and DomainID (DID)
-------------------------------------

If the platform supports aliases and multiple domains, the canonical user name and domain
name may not be available when sending the INVITE request to the voicemail/voicebox server.
Also, domain ID is case insensitive, thus iptel.org and Iptel.org may appear as different 
domains to the case sensitive msg_storage engine.
Thus, the user and domain may be overridden by user id and domain id. These values are not 
used in the email template, they are used only for selection of the user's voicebox and 
personal greeting message.


Voicebox mode
-------------

This mode uses a msg_storage plugin to save the message to disk. This way it can be 
saved so that users can call in to listen to the messages. If no msg_storage module 
is loaded, Mode=box and Mode=both is not available. See documentation of msg_storage 
voicebox plug-ins for more details.

Voicemail email templates 
-------------------------

The format of the email can be configured using email templates.  Email
templates will help you to customize the emails sent by Voicemail. Just
configure the path where the templates can be found and edit your
template files.  You will find detailed informations in the following
sections.

Configuration parameters within voicemail.conf:
----------------------------------------------

This is needed in template files are kept in file system:

email_template_path: 

	- sets the directory which will be searched for
	  email template files (*.template).

	- each template file stands for a different domain:
	    <domain>.template
	    Ex: iptel.org.template

	- additionaly, a default template must be present in
	  the directory configured with that configuration
	  parameter. It must be named 'default.template'.

If you configured email_template_path well and turned debug informations
on, you should find in the log file during loading process following lines:

(9834) DEBUG: loadEmailTemplates (AnswerMachine.cpp:102): loading plug-in/voicemail/default.template ...
(9834) DEBUG: loadEmailTemplates (AnswerMachine.cpp:102): loading plug-in/voicemail/iptel.org.template ...
(9834) DEBUG: loadEmailTemplates (AnswerMachine.cpp:102): loading plug-in/voicemail/susie.at.template ...

If template files are kept in MySQL database, they are looked for first
from domain_template table and then from default_template table, which
are defined as follows:

CREATE TABLE domain_template (
  id int(10) unsigned NOT NULL,
  application varchar(32) NOT NULL,
  message varchar(32) NOT NULL,
  domain varchar(128) NOT NULL,
  `language` char(2) NOT NULL default '',
  template text NOT NULL,
  PRIMARY KEY  (id)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

CREATE TABLE default_template (
  id int(10) unsigned NOT NULL auto_increment,
  application varchar(32) NOT NULL,
  message varchar(32) NOT NULL,
  `language` char(2) NOT NULL default '',
  template text NOT NULL,
  PRIMARY KEY  (id)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1;

Application value is 'voicemail' and message value is 'email_tmpl'.
Language value is a two letter code of a language or '' if language is
unknown.

When an audio file is fetched from database, it is stored in /tmp
directory.

Email template format:
----------------------

to: <destination email address>
from: <source email address>
subject: <subject line>
[header: <additional header>]
<empty line>
<body>

The optional header line adds a freely configurable 
header to the mail. For example, the voicemail can be 
securely filtered for the header X-Content if the following 
is used in the voicemail template:
header: X-Content: Voicemail
This line is optional. 

Multiple headers can be set by inserting  a \n, for example
header: X-Content: Voicemail\nX-Anotherheader: %domain%

Variables:
----------

If you want to customize the email depending on call datas, 
please use folowing syntax: %variable%

Following variables are supported:

 - %to% : local URI.
 - %domain% : local domain.
 - %user% : local user.
 - %email% : email address of the local user.

 - %from%/%sender% : Sender or remote URI.
 - %from_user% : remote user.
 - %from_domain% : remote domain.

 - %vmsg_length% : voice message length in seconds

 - %uid% : user id
 - %did% : domain id
 - %ts%  : unix timestamp (seconds)
