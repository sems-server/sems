/*! \page howtostart_noproxy How to try out SEMS without setting up a proxy

  \section Introduction
  
   <p>
   This text describes how one can try out services in SEMS without 
   setting up a proxy. This is the simplest way to try services in SEMS, 
   or start with developing a service.
   </p>
   
   <p>
   The way this works is that SEMS registers to a SIP server (registrar) with one account (bob),
   just like any other SIP phone, and we call SEMS from another account (alice). If the
   SIP server provides DID calling from the PSTN, we can use any landline or mobile
   phone for testing or using the service.
  </p>   
      
   \note What is not possible with this method is to use applications which need
   additional information for a call from the subscriber data. For example,
   in order to send a voicemail as email the SEMS server needs the email 
   address to send the mail to.
      
  \section Requirements
  
  For compiling SEMS, as a minimum a C++ compiler and make is needed. In debian,
  do \code apt-get install g++ make \endcode
  
  \subsection publicsip With a public SIP server
  Two accounts at a public SIP server are needed.
  We recommend to use iptel.org's SIP service for testing, an account 
  can be registered for free at <a href="http://iptel.org/service/">http://iptel.org/service/</a>.
  
  Any SIP phone, hardphone or softphone, can be used for testing. Cross-platform, 
  <a href="http://www.sip-communicator.org">sip-communicator</a> is recommended, for Linux 
  <a href="http://twinklephone.com">twinkle</a>, for Windows sip-communicator,
  xten eyebeam or NCH express.
  
  \subsection pstnsip With a PSTN DID provider
  Alternatively a PSTN DID provider can be used. In that case, we can test and use the 
  service with any phone. A list of DID providers is for example available at 
  <a href="http://www.voip-info.org/wiki/view/DID+Service+Providers">voip-info</a> . 
  <a href="http://sipgate.de">Sipgate</a> for example, provides free DID numbers in 
  Germany.
  
  
  \section Installing Installing SEMS from source
   First, the SEMS source is downloaded from iptel.org and extracted:
   \code
     $ wget ftp.iptel.org/pub/sems/sems-latest.tar.gz
     $ tar xzvf sems-latest.tar.gz
   \endcode
   SEMS is compiled:
   \code
    $ cd sems-x.y.z/
    $ make
   \endcode
  \note Compilation may fail for some modules due to missing dependencies. 
  For most modules, that can be ignored for the moment.
  
  Then SEMS is installed: 
  \code
    $ make install
  \endcode
  
  This will install
    - configuration in /usr/local/etc/sems/    
    - the sems binary in /usr/local/sbin/sems
    - modules in /usr/local/lib/sems/plug-in/
    - audio files in /usr/local/lib/sems/audio/
      
  \section Configuring_application Configuring the application for SEMS 
  
  There are many many modules shipped with SEMS, applications like announcement, voicemail, 
  conference, etc, codec modules, and some things like SIP registrar client. 
  
  Now  we configure SEMS to load the conference application and execute the conference application
  for incoming calls. We also set it to have itself register to our SIP server.
    
  In <b>/usr/local/etc/sems/sems.conf</b>, we set
  \code
   load_plugins=wav;uac_auth;registrar_client;reg_agent;conference   
  \endcode
  <p>  to load the modules we need; wav is for reading WAV files and for the G711 codec,
  uac_auth is the module which implements authentication, registrar_client facilitates registration at a SIP server, 
  and reg_agent is the application that uses registrar_client to have SEMS register at a SIP server.</p>

  <p> We also set </p>
  \code
   application=conference
  \endcode
  <p> so that SEMS executes the conference application for an incoming call.</p>
  
  <p> We want SEMS to register at a SIP server, so we need to tell it about the user name and the password, this is set 
   in <b>/usr/local/etc/sems/reg_agent.conf</b> (of course this user name bob and the password need to be set to the
   ones used for testing):</p>
  \code
     domain=iptel.org
     user=bob
     display_name=bob
     auth_user=bob
     pwd=verysecret
  \endcode
  
  \section Trying Testing the setup
  Now we can test the configuration by running SEMS from the command line like this:
  \code
    /usr/local/sbin/sems -f /usr/local/etc/sems/sems.conf -D 3 -E
  \endcode
 <p>  <b>-D 3</b> sets the debug level higher so that we see what is going on, and <b>-E</b> makes SEMS start in the foreground
  and go to daemon mode. It also makes the log appear on the terminal and not in the system log file.</p>

 <p>  If everything is alright, SEMS starts up with a lot of messages, and hopefully no ERROR. There should also be some messages
 appearing which show that SEMS registered successfully to the SIP server.
 </p>

 <p> Now we can call bob from the other phone or our PSTN telephone. In the SEMS log, we see the call appearing, and on the phone
  we hear a message saying that we are the first participant in the conference.</p>

 <p> If it doesnt work.... we examine the log for the ERROR that occured. Possibly, depending on the network setup,
 we need to change the interface that SEMS is running on; this can be changed by setting the <b>media_ip</b> and 
 <b>sip_ip</b> options in sems.conf. Also, it might be that there is already someone using that port (default config: 5070),
 in that case <b>sip_port</b> needs to be set. </p>
 
  \section Running Running as daemon
   If SEMS is started without the <b>-E</b> option, it will continue running as daemon in the background. The log can be seen in 
   syslog (e.g. with <b> tail -f /var/log/daemon.log</b>).

  \section Other_application Running other applications
   If we want to run other applications, the <b>load_plugins=</b> and <b>application=</b> parameters need to be adapted.
   See \ref AppDoc for a description of the shipped applications.
   
  \section DSM_application Creating and running a simple DSM applications
  <p>
    The DSM is a service development platform, that makes it simple to create powerful services. The service logis is defined
    as a state machine, and the DSM application interprets this state machine for the calls, evaluating when to change state,
    and which actions to execute.    
  </p>
  
  To use a DSM application, we set in <b>/usr/local/etc/sems/sems.conf </b>
  \code
   load_plugins=wav;uac_auth;registrar_client;reg_agent;session_timer;dsm
   application=mydsmapp
  \endcode
  
  and in 
  <b>/usr/local/etc/sems/dsm.conf </b>: 
  \code
    diag_path=/usr/local/lib/sems/dsm/
    load_diags=mydsmapp
    register_apps=mydsmapp
  \endcode
  
  Then we paste this little script in /usr/local/lib/sems/dsm/mydsmapp.dsm :
  \code
  initial state BEGIN
   enter {
     playFile(/usr/local/lib/sems/audio/webconference/first_participant.wav);
   };
  transition "file ends" BEGIN - noAudioTest -> TYPING;
  
  state TYPING;
  transition "typed a key" TYPING - keyTest(#key < 10) / {
    set($myfile=/usr/local/lib/sems/audio/webconference/);
    append($myfile, #key);
    append($myfile, .wav);
    playFile($myfile);
  } -> TYPING;

  transition "BYE received" (BEGIN, TYPING) - hangup / stop(false) -> END;
  state END; 
  \endcode
  
  This little script welcomes the caller, and then plays the key that the caller entered. More documentation about DSM and 
  examples are in apps/dsm/doc, and also \ref ModuleDoc_dsm .

*/
