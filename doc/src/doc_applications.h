/*! \page AppDoc Application Modules
 *  Documentation for the applications that come with SEMS. The applications 
 *  can be found in the apps/ directory and are installed by default if they do not
 *  depend on special libraries (eg. liblame). 
 *  A set of \ref AppDocExample that illustrate how to
 *  make use certain aspects of the SEMS framework can be found in the
 *  apps/examples directory. These are not installed by default. 
 *  
 * \section announcementappdoc Announcement  Applications
 *  Applications that play announcements to the caller. 
 *  For plain announcements, there is the <i>announcement</i> module. 
 *
 *  <ul><li>\ref ModuleDoc_announcement </li></ul>
 *
 *  Pre-call announcements can either be implemented using early media with the 
 *  <i>early_announce</i> application,
 *
 *  <ul><li>\ref ModuleDoc_early_announce </li></ul>
 *
 *  or the session is established and after the announcement SEMS acts as B2BUA,
 *  inviting the original r-uri, and finally reinviting the caller: 
 *
 *   <ul><li>\ref ModuleDoc_ann_b2b </li></ul>
 *
 *  Another possibility is to establish the session and then REFER the caller: 
 *   <ul><li> \ref ModuleDoc_announce_transfer </li></ul>
 * 
 *  As SEMS can also do UAC authentication for a call using the <i>uac_auth</i> 
 *  component plugin (\Ref ModuleDoc_uac_auth). An example where this is used 
 *  is the <i>announce_auth</i> example application: 
 * <ul><li>  \ref ModuleDoc_announce_auth </li></ul>
 *
 * \section voicemailboxappdoc Voicemail and Mailbox
 * SEMS has a <i>voicemail</i> application, which send a recorded message via 
 * Email (voicemail2email), saves the message to the voicebox, or does both: 
 *
 *  <ul><li> \ref ModuleDoc_voicemail </li></ul>
 * 
 * Messages saved to voicebox can be listened to using the voicebox application: 
 *  <ul><li> \ref ModuleDoc_voicebox </li></ul> 
 * 
 * The annrecorder application can be used to record a personal greeting message.: 
 *  <ul><li> \ref ModuleDoc_annrecorder </li></ul> 
 * 
 * There is also a simpler mailbox application, which stores recorded messages (in an IMAP 
 * server) and users can dial in to check their messages: 
 * 
 *  <ul><li> \ref ModuleDoc_mailbox </li></ul>
 * 
 * \section conferencingappdoc Conferencing
 * SEMS can be a conference bridge with the <i>conference</i> application: 
 *
 *   <ul><li> \ref ModuleDoc_conference </li></ul>
 * 
 * \subsection webconferencingappdoc  Web controlled conference  rooms
 * 
 *  Using the webconference application, conference rooms can be controlled
 *  from e.g. a web control page, or some other external mechanism: 
 * 
 *   <ul><li> \ref ModuleDoc_webconference </li></ul>
 * \subsection conferencingauthappdoc  Authentication for conference rooms (PIN entry) 
 *
 *  There are two possibilies how a PIN entry for conference rooms (or for
 *  other services) can be implemented: after the PIN is collected and verified 
 *  against a XMLRPC authentication server, the call can be connected to 
 *  the conference room either using B2BUA, or it can be transfered to the 
 *  conference bridge using a (proprietary) REFER call flow. The b2bua
 *  solution, which also gives the possibility to limit the call time, is
 *  implemented in the <i>conf_auth</i> plugin: 
 *
 *   <ul><li> \ref ModuleDoc_conf_auth </li></ul>
 *
 *  The other call flow can be implemented using the <i>pin_collect</i> 
 *  application: 
 * <ul><li>  \ref ModuleDoc_pin_collect </li></ul>
 *
 *
 * 
 * \section Back-to-Back User Agent (B2BUA) and SBC applications
 *
   SEMS has a powerful and flexible B2BUA application, called 'sbc', which
   implements flexible routing options, header, message and codec filter,
   optional RTP relay, SIP authentication, Session Timers, prepaid accounting
   and call timer etc.

    <ul><li> \ref ModuleDoc_sbc </li></ul>

 *
 * \section Click2Dial 
 *
 * An xmlrpc-enabled way to initiate authenticated calls: 
 *
 *   <ul><li> \ref ModuleDoc_click2dial </li></ul>
 *
 * \section dsmapp Defining and developing applications as state machine charts
 * 
 *  The DSM module allows to define an application as simple, easy to read, 
 *  self-documenting, concise state diagram. This state machine definition is then 
 *  interpreted and executed by the DSM application. 
 * 
 *  <ul><li>  \ref ModuleDoc_dsm </li></ul>
 * 
 * \section pythonscripting Scripting SEMS with Python 
 *
 * There are two application modules which embed a python interpreted into 
 * SEMS: the <i>ivr</i> module and the <i>py_sems</i> module.
 * 
 * The <i>ivr</i> module plugin embeds a python interpreter into SEMS. In it, 
 * applications written in python can be run (<i>mailbox</i>, <i>conf_auth</i>,
 * <i>pin_collect</i> for example) and new applications can be prototyped and 
 * implemented very quickly: 
 *
 * <ul><li> \ref ModuleDoc_ivr  </li></ul>
 * 
 * The <i>ivr</i> module has a simple to use, yet limited API, which uses 
 * hand-written wrappers for the python bindings.
 *
 * <i>py_sems</i> uses a binding generator to make python classes from the
 * SEMS core C++ classes, thus exposing a lot more functionality natively 
 * to python:
 *
 * <ul><li> \ref ModuleDoc_py_sems  </li></ul>
 * 
 * \section registrar_client Registering SEMS at a SIP registrar
 * 
 * The <i>reg_agent</i> module together with the <i>registar_client</i> module
 * can be used to register at a SIP registrar.
 * 
 * <ul><li> \ref ModuleDoc_reg_agent  </li></ul>
 * <ul><li> \ref ModuleDoc_registrar_client  </li></ul>
 *
 * \section various_apps Various applications
 * 
 * xmlrpc2di (\ref ModuleDoc_xmlrpc2di) exposes DI interfaces as XMLRPC server.
 * This is very useful to connect SEMS with other software, that e.g. trigger click2dial
 * calls, create registrations at SIP registrar, do monitoring, etc.  
 * 
 * jsonrpc (\ref ModuleDoc_jsonrpc) exposes DI interfaces as json-rpcv2 server.
 *
 * callback application can save lots of mobile calls costs, it calls back caller 
 * and then the caller can enter a number to be connected to:
 *
 *  <ul><li> \ref ModuleDoc_callback </li></ul>
 *
 *
 * \section morecomponents Other components
 *
 *  <ul><li> \ref ModuleDoc_diameter_client </li></ul>
 *
 *  <ul><li> \ref ModuleDoc_monitoring </li></ul>
 */


/*! \page ModuleDoc_ann_b2b Module Documentation: ann_b2b Application 
 *  \section Readme_ann_b2b Readme file
 *  \verbinclude Readme.ann_b2b.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_announce_auth Module Documentation: announce_auth Application 
 *  \section Readme_announce_auth Readme file
 *  \verbinclude Readme.announce_auth.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_announce_transfer Module Documentation: announce_transfer Application 
 *  \section Readme_announce_transfer Readme file
 *  \verbinclude Readme.announce_transfer.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_announcement Module Documentation: announcement Application 
 *  \section Readme_announcement Readme file
 *  \verbinclude Readme.announcement.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_conference Module Documentation: conference Application 
 *  \section Readme_conference Readme file
 *  \verbinclude Readme.conference.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_webconference Module Documentation: webconference Application 
 *  \section Readme_webconference Readme file
 *  \verbinclude Readme.webconference.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_early_announce Module Documentation: early_announce Application 
 *  \section Readme_early_announce Readme file
 *  \verbinclude Readme.early_announce.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_voicemail Module Documentation: voicemail Application 
 *  \section Readme_voicemail Readme file
 *  \verbinclude Readme.voicemail.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_voicebox Module Documentation: voicebox Application 
 *  \section Readme_voicebox Readme file
 *  \verbinclude Readme.voicebox.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_annrecorder Module Documentation: annrecorder Application 
 *  \section Readme_annrecorder Readme file
 *  \verbinclude Readme.annrecorder.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */


/*! \page ModuleDoc_mailbox Module Documentation: mailbox Application 
 *  \section Readme_mailbox Readme file
 *  \verbinclude Readme.mailbox.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_ivr Module Documentation: ivr Application 
 *  \section Readme_ivr Readme file
 *  \verbinclude Readme.ivr.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_py_sems Module Documentation: py_sems Application 
 *  \section Readme_py_sems Readme file
 *  \verbinclude Readme.py_sems.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_reg_agent Module Documentation: reg_agent Application 
 *  \section Readme_reg_agent Readme file
 *  \verbinclude Readme.reg_agent.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_registrar_client Module Documentation: registrar_client Application 
 *  \section Readme_registrar_client Readme file
 *  \verbinclude Readme.registrar_client.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_uac_auth Module Documentation: uac_auth component 
 *  \section Readme_uac_auth Readme file
 *  \verbinclude Readme.uac_auth.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_mp3plugin Module Documentation: mp3 file writer audio plugin
 *  \section Readme_mp3plugin Readme file
 *  \verbinclude Readme.mp3plugin.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_iLBC Module Documentation: iLBC codec plugin
 *  \section Readme_iLBC Readme file
 *  \verbinclude Readme.iLBC.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_callback Module Documentation: callback application plugin
 *  \section Readme_callback Readme file
 *  \verbinclude Readme.callback.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_auth_b2b Module Documentation: auth_b2b application plugin
 *  \section Readme_auth_b2b Readme file
 *  \verbinclude Readme.auth_b2b.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_sst_b2b Module Documentation: sst_b2b application plugin
 *  \section Readme_sst_b2b Readme file
 *  \verbinclude Readme.sst_b2b.txt
 *
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_diameter_client Module Documentation: diameter_client component plugin
 *  \section Readme_diameter_client Readme file
 *  \verbinclude Readme.diameter_client.txt
 *
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */


/*! \page ModuleDoc_monitoring Module Documentation: monitoring in-memory DB component plugin
 *  \section Readme_monitoring Readme file
 *  \verbinclude Readme.monitoring.txt
 *
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_sw_prepaid_sip Module Documentation: prepaid_sip application plugin
 *  \section Readme_prepaid_sip Readme file
 *  \verbinclude Readme.sw_prepaid_sip.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_click2dial Module Documentation: click2dial application plugin
 *  \section Readme_click2dial Readme file
 *  \verbinclude Readme.click2dial.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_xmlrpc2di Module Documentation: xmlrpc2di Application 
 *  \section Readme_xmlrpc2di Readme file
 *  \verbinclude Readme.xmlrpc2di.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_call_timer Module Documentation: call_timer Application 
 *  \section Readme_call_timer Readme file
 *  \verbinclude Readme.call_timer.txt
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */




/*! \page ModuleDoc_dsm DSM: State machine notation for VoIP applications 
 *  
 *  \section Readme_dsm DSM Readme file 
 *  <p>
 *  \verbinclude Readme.dsm.txt
 *  </p>
 *  \section dsm_syntax DSM Syntax
 *  <p>
 *  \verbinclude dsm_syntax.txt
 *  </p>
 *  \section dsm_errorhandling Error handling in DSM
 *  <p>
 *  \verbinclude dsm_errorhandling.txt
 *  </p>
 *  \section dsm_todo DSM Todo
 *  <p>
 *  \verbinclude dsm_todo.txt
 *  </p>
 *
 *  \section dsm_mods DSM modules
 *  \subsection dsm_mod_sys mod_sys - System  functions
 *  <p>
 *  \verbinclude Readme.mod_sys.txt
 *  </p>
 *
 *  \subsection dsm_mod_dlg mod_dlg - Dialog related functionality
 *  <p>
 *  \verbinclude Readme.mod_dlg.txt
 *  </p>
 *
 *  \subsection dsm_mod_uri mod_uri - URI parser and related functionality
 *  <p>
 *  \verbinclude Readme.mod_uri.txt
 *  </p>
 *
 *  \subsection dsm_mod_utils mod_utils - Utility functions
 *  <p>
 *  \verbinclude Readme.mod_utils.txt
 *  </p>

   \subsection dsm_mod_monitoring mod_monitoring - Monitoring functions
   <p>
   \verbinclude Readme.mod_monitoring.txt
   </p>

 *  \subsection dsm_mod_conference mod_conference - Conferencing functions
 *  <p>
 *  \verbinclude Readme.mod_conference.txt
 *  </p>
 * 
 *  \subsection dsm_mod_mysql mod_mysql - MySQL DB access
 *  <p>
 *  \verbinclude Readme.mod_mysql.txt
 *  </p>
 * 
 *  \subsection dsm_mod_py mod_py - Embedded Python functions
 *  <p>
 *  \verbinclude Readme.mod_py.txt
 *  </p>
 * 
 *  \subsection dsm_mod_aws mod_aws - Amazon Web Services functions
 *  <p>
 *  \verbinclude Readme.mod_aws.txt
 *  </p>
 * 

   \subsection dsm_mod_xml mod_xml - XML utility functions
   <p>
   \verbinclude Readme.mod_xml.txt
   </p>

 * 
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */
