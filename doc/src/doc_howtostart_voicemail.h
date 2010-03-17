
/*! \page howtostart_voicemail How to set up the proxy for voicemail and voicebox in SEMS

  \section Introduction
  
   <p>
   This text describes how one can set up a SER based home proxy SIP proxy with voicemail and voicebox service 
   implemented in SEMS. </p>

   <p>With minor modifications, this should work with home proxies implemented with 
   SER derivatives (<a href="http://kamailio.org">Kamailio</a> 1.x, <a href="http://opensips.org">OpenSIPS</a>), 
   and also with <a href="http://sip-router.org">sip-router</a> (e.g. Kamailio 3.0) based proxy configurations. For other types of 
   proxies or SIP platforms, it should give an idea of what is required to use a SEMS based voicemail system.
   </p>

   \section voicemail_in_sems Features of a voicemail system with SEMS
   <p>
   The voicemail system that comes with SEMS supports the following features
     - voicemail2email and/or dial-in voicebox 
     - greeting only mode
     - voicebox plays message count
     - new and saved messages
     - user can record personal greeting message (as a separate service number)
     - multi-domain capable 
     - multi-language capable (e.g. as user setting), supports single-digits pre and post
     - supports domain and user aliases (domain/user string or domain-ID (DID)/user-ID (UID) )
     - prompts per domain/language
     - default greeting message per domain/language
     - configurable key bindings for menu     
   </p>   

   \section voicemail_parameters Parameters to voicemail applications
   <p>
   Usually, when a call should be sent to the voicemail system, the home proxy already knows some parts or all 
   of the user profile, for example the email address of a user, or the voicemail settings; for example the 
   user profile is already loaded from a DB (or LDAP, RADIUS, DIAMETER etc). For this reason, in a
   SEMS based voicemail system, the proxy adds the relevant information as parameters to the INVITE request. 
   Those parameters are set in the P-App-Param header. 
   </p>
  <p>
  Example:
   \verbatim   
     INVITE sip:1000@sems01.iptel.org:5080 SIP/2.0.                                                                                    
     From: "sayer@iptel" <sip:sayer@iptel.org>;tag=d3olt2dqvl.                                                                      
     To: <sip:1000@iptel.org>.                                                                                                      
     ...
     P-App-Name: voicebox.                                                                                                          
     P-App-Param: usr=sayer;dom=iptel.org;lng=en;uid=3ab0a114-ceff-11da-8607-0002b3abca3a;did=2f2091f5-ceff-11da-8220-0002b338cf3a;.
   \endverbatim      
  </p>
  
  <p>
  If the proxy does not support this, or does not have access to the user profile, there are two solutions:
    - add another SER-based proxy in front of SEMS that has access to user profile, and adds those headers
    - add the functionality for accessing the user profile to SEMS (e.g. access DB in SEMS)

  For both solutions, the main complexity lies in the fact that the right user needs to be identified (with support for 
  multi domain, aliases, call forwarding etc).
  </p>
  
   \section voicemail_in_sems Components of voicemail and voicebox system in SEMS
   <p>
   There is three applications involved in a voicemail/voicebox system in SEMS: <em>voicemail</em>, <em>voicebox</em> 
   and <em>annrecorder</em>. Voicemail is the application that records a message, and sends the message as email or 
   stores it into the voicebox storage. Voicebox is the application that users can dial into, listen to their messages,
   delete or save them. Annrecorder is an application that lets users record their personal greeting message.   
   </p>
   
   <p>
   If only voicemail2email is to be used, the voicemail application alone can be employed. In that case, the mode 
   must be set to voicemail (see voicemail application parameters below). 
   </p>   
   
   \section msg_Storage Storage for voice message files and greetings
   <p>
   The storage for voice messages is implemented in a separate module. This way for example a specialized adapter 
   to some replicated storage system can be implemented and loaded without changing the other applications. 
   </p>
   <p>
   A storage module only needs to support a few very simple functions: Create, get and delete messages, mark a message
   as read, list a user's directory, and get the number of messages in the user's directory. The sender and the message
   record time is encoded in the message name.
   </p>
   <p>
   The default storage module, <a>msg_storage</a>, is an implementation that just uses the normal file system 
   calls (fopen(), readdir(), opendir() etc). As 'saved' flag, the mtime of the file is compared to the atime.
   \note If your file system does not support atime, this will not work, i.e. all messages will always appear as unread!
   </p>
   
   \section did_uid Domain/User text or domain ID (DID) and user ID (UID)   
   <p>
   If the platform supports user and domain aliases (e.g. sip.iptel.org and iptel.org, or numeric aliases), there may not be
   a canonical user name available. For that case, the user ID and domain ID (canonical user/domain ID) may be used, by setting
   UID/DID application parameters. This overrides the user name and domain name, so that the correct user and domain is identified.
   </p>

   \section vm_modes Voicemail application modes
   <p>
   The voicemail application has four modes: 
      - voicemail  : send email (default)
      - box        : leave in voicebox (store in msg_storage)
      - both       : send email and leave in voicebox
      - ann        : just play greeting, don't record message.
      
   For <em>voicemail</em> and <em>both</em> mode, the email address must be given as parameter.
   </p>
   
   \section vm_avps Voicemail specific AVPs
   The following user AVPs should be configured in SerWeb to be user-configurable:
   - voicemail :   voicemail mode - 'voicemail', 'box', 'both', or 'ann'
   - email:        email address
   - lang:         language - selectable from those for which prompts are present
   
   \section ser_commands Proxy configuration for ser-oob.cfg 

   These route fragments could be inserted into a typical ser-oob or default Kamailio configuration. 
   
   
   \subsection leaving_message Leaving a message
   This should be added to native SIP destinations which are not found in usrloc, i.e. instead of replying 
   480 User temporarily not available, and in FAILURE_ROUTE:
   
   \verbatim
       append_hf("P-App-Name: voicemail\r\n");
       append_hf("P-App-Param: mod=%$t.voicemail%|;eml='%$t.email%|';usr=%@ruri.user%|;snd='%@from.uri%|';dom=%@ruri.host%|;uid=%$t.uid%|;did=%$t.did%|;");
       rewritehostport("voicemail.domain.net:5080");
       route(FORWARD);
   \endverbatim

   \subsection calling_voicebox Calling voicebox
   This should be added to SITE-SPECIFIC route:
   \verbatim          
     if (uri=~"^sip:1000") {               # 1000 is voicebox access number
         append_hf("P-App-Name: voicebox\r\n");
         append_hf("P-App-Param: usr=%@from.uri.user%|;dom=%@from.uri.host%|;lng=%$f.lang%|;uid=%$f.uid%|;did=%$f.did%|;\r\n");
         rewritehostport("voicemail.domain.net:5080");
         route(FORWARD);
     }
   \endverbatim
   
   \subsection calling_annrecorder Recording the greeting
   This is very similar to the one above, and should be added to SITE_SPECIFIC as well:
   \verbatim
     if (uri=~"^sip:1001") {               # 1001 is recod greeting number
         append_hf("P-App-Name: annrecorder\r\n");
         append_hf("P-App-Param: usr=%@from.uri.user%|;dom=%@from.uri.host%|;lng=%$f.lang%|;uid=%$f.uid%|;did=%$f.did%|;typ=vm;\r\n");
         rewritehostport("voicemail.domain.net:5080");
         route(FORWARD);
     }
   \endverbatim
   Note the type (typ) here; the annrecorder application can be used to record different greetings (e.g. away greeting when 
   recording message, or normal away greeting). This type can be used when sending a call to <em>voicemail</em> application.
*/