/* \file This file generates Doxygen pages from files in the /doc
 *  directory, and from the readme files in the apps/examples dir.
 */

/*! \page index SEMS Documentation 
 *  \section news News & Changes
 *  \arg \ref whatsnew_0100 
 *  \arg \ref changelog 
 *
 *  \section general General
 *  \arg \ref Readme 
 * 
 *  \section userdoc User's documentation
 *  \arg \ref sems.conf.sample
 *  \arg \ref Compiling 
 *  \arg \ref Configure-Sems-Ser-HOWTO
 *  \arg \ref AppDoc
 *
 *  \section developerdoc Developer's documentation
 *   \arg <a href="http://www.iptel.org/files/semsng-designoverview.pdf">
 *         SEMS Design Overview</a>
 *  \arg <a href="http://www.iptel.org/sems/sems_application_development_tutorial"> 
 *    Application Development Tutorial </a>
 *  \arg \ref AppDocExample
 *  \arg \ref ComponentDoc
 *
 *  \section weblinks Web sites
 *    \arg \b Main:  SEMS website http://iptel.org/sems
 *    \arg \b sems & semsdev Lists: List server http://lists.iptel.org
 *    \arg \b Bugs:  Bug tracker: http://tracker.iptel.org/browse/SEMS
 *
 */

/*! \page whatsnew_0100 What's new in SEMS 0.10.0
 * SEMS has changed a lot between 0.9 versions and 0.10 versions.
 * The file WHATSNEW lists the most important changes:
 * \verbinclude WHATSNEW
 * 
 */

/*! \page changelog Changelog (from 0.10.0-rc1 onwards)
 * CHANGELOG for SEMS from 0.10.0-rc1 onwards 
 * \verbinclude CHANGELOG
 * 
 */

/*! \page Readme SEMS Readme file
 * SEMS Readme file:
 * 
 * \verbinclude README
 * 
 */

/*! \page Compiling Compilation instructions
 * \verbinclude COMPILING
 * 
 */

/*! \page Configure-Sems-Ser-HOWTO mini-Howto on how to configure SER and SEMS to work together
 *
 * Be sure to also check the instructions that are on the page with the 
 * <a href="http://www.iptel.org/sems/sems_application_development_tutorial">
 * application development tutorial</a>, there is step by step instructions including 
 * a complete set of configuration files that should get you started with SEMS 
 * very quickly.
 *
 * \verbinclude Configure-Sems-Ser-HOWTO
 * 
 */

/*! \page sems.conf.sample SEMS core configuration parameters
 * <p>The sample configuration file core/etc/sems.conf.sample 
 * explains all core configuration parameters. </p><p>If there is no 
 * configuration file present, 'make install' installs this file
 * into the default location.</p>
 * 
 * \verbinclude sems.conf.sample
 * 
 */

/*! \page AppDoc Application Modules Documentation
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
 * Email (voicemail2email): 
 *
 *  <ul><li> \ref ModuleDoc_voicemail </li></ul>
 * 
 * There is also a mailbox application, which stores recorded messages (in an IMAP 
 * server) and users can dial in to check their messages: 
 * 
 *  <ul><li> \ref ModuleDoc_mailbox </li></ul>
 * 
 * \section conferencingappdoc Conferencing
 * SEMS can be a conference bridge with the <i>conference</i> application: 
 *
 *   <ul><li> \ref ModuleDoc_conference </li></ul>
 * 
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
 * \section Prepaid 
 *
 * This is a signalling-only prepaid engine.
 *
 *   <ul><li> \ref ModuleDoc_sw_prepaid_sip </li></ul>
 *
 * \section Click2Dial 
 *
 * An xmlrpc-enabled way to initiate authenticated calls: 
 *
 *   <ul><li> \ref ModuleDoc_click2dial </li></ul>
 *
 * \section Scripting SEMS with Python 
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
 * \section Registering SEMS at a SIP registrar
 * 
 * The <i>reg_agent</i> module together with the <i>registar_client</i> module
 * can be used to register at a SIP registrar.
 * 
 * <ul><li> \ref ModuleDoc_reg_agent  </li></ul>
 * <ul><li> \ref ModuleDoc_registrar_client  </li></ul>
 *
 * \section Various applications
 * 
 *  <ul><li> \ref ModuleDoc_callback </li></ul>
 */

/*! \page AppDocExample Example Applications
 * 
 * These are examples that illustrate how to
 * make use certain aspects of the SEMS framework rather than
 * be complete ready-to-use applications. The examples given
 * here can be a good start for further development, though, and some,
 * like the xmlrpc2di, cacheannounce, db_announce, serviceline and the
 * calling card application might be used right out of the box.
 * 
 * \section exampleapptutorialdoc The examples from the tutorial
 *
 * The <a href="http://www.iptel.org/sems/sems_application_development_tutorial">
 * application development tutorial</a> has some useful examples and 
 * also some useful templates to start development of applications. 
 * 
 * myapp (\ref ModuleDoc_myapp) is just an empty template of a SEMS application.  
 * myconfigurableapp (\ref ModuleDoc_myconfigurableapp) adds configurability and shows how to read the
 * config parameters from the module config file.  myannounceapp (\ref ModuleDoc_myannounceapp) then
 * plays an announcement to the caller, similar to the announcement plugin, but 
 * without searching paths etc. 
 *
 * The myjukebox (\ref ModuleDoc_myjukebox) is an example how to get DTMF input and react on it.
 * It plays the corresponding file if a key is pressed on the telephone. 
 *
 * The calling card example then implements a calling card service - the caller 
 * enters a PIN number, and then the number to call, is connected in b2bua mode, 
 * and if the credit is expired the call is disconnected. The application is the 
 * mycc application (\ref ModuleDoc_mycc), and the accounting for this is done in 
 * and extra module, cc_acc (\ref ModuleDoc_cc_acc), which is used via the internal
 * DI interface. Another implementation, the cc_acc_xmlrpc (\ref ModuleDoc_cc_acc_xmlrpc),
 * asks an external xmlrpc accounting server.
 *
 * Python examples are two in the tutorial: One is a "hello world" (\ref ModuleDoc_ivr_announce),
 * and the other is (a part of) RFC4240 announcement service, annc_service (\ref ModuleDoc_annc_service).
 *
 * \section otherexampleappdoc Other example applications
 * 
 * xmlrpc2di (\ref ModuleDoc_xmlrpc2di) exposes DI interfaces as XMLRPC server.
 * This is very useful to connect SEMS with other software, that e.g. trigger click2dial
 * calls, create registrations at SIP registrar, do monitoring, etc.  
 *
 * di_dial (\ref ModuleDoc_di_dialer) triggers outgoing calls, i.e. calls that originate from SEMS.
 * It can provide credentials from dialout PINs for authentication. 
 * 
 * announce_auth (\ref ModuleDoc_announce_auth) makes an authenticated call and plays, like the
 * announcement module, a sound file. It is a simple example of how to use the uac_auth for 
 * authentication.
 *
 * db_announce (\ref ModuleDoc_db_announce) and cacheannounce (\ref ModuleDoc_cacheannounce) are
 * replacements for the announcement application: db_announce get the announcement file it plays 
 * from DB via SQL query, cacheannounce loads the announcement to play at startup, and plays it from 
 * memory.
 *
 * di_log (\ref ModuleDoc_di_log) implements the logging interface - it saves the last n log lines
 * in an in-memory ring buffer, which can be retrieved via DI (using xmlrpc2di via XMLRPC).
 * 
 * jukecall (\ref ModuleDoc_jukecall) does a b2bua call to the callee with SEMS in the
 * media path, to be able to play a file into the call when the caller presses a 
 * button. This demonstrates how to use the so called b2abua session type. 
 *
 * mixin_announce (\ref ModuleDoc_mixin_announce) periodically mixes a second file into an
 * announcement played, an example on how to use AmAudioMixIn.
 * 
 * simple_conference (\ref ModuleDoc_simple_conference) and pinauthconference 
 * (\ref ModuleDoc_pinauthconference) are simpler versions of the conference plugin, which are good
 * templates to start with programming if a special conference service should be implemented.
 * simple_conference is just plain conference mixer/bridge (without dialout etc), pinauthconference 
 * asks the conference room first.
 * 
 * The serviceline (\ref ModuleDoc_serviceline) is an application implementing a service 
 * line (press one to get to A, two for B, ...) with ivr, b2bua and SIP auth.
 * 
 * The py_sems_ex directory contains some example for the py_sems embedded Python interpreter.
 *
 * The early_record (\ref ModuleDoc_early_record) is an example on how to receive and use early media. 
 * 
 */

/*! \page ModuleDoc_ann_b2b Module Documentation: ann_b2b Application 
 *  \section Readme_ann_b2b Readme file
 *  \verbinclude Readme.ann_b2b
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_announce_auth Module Documentation: announce_auth Application 
 *  \section Readme_announce_auth Readme file
 *  \verbinclude Readme.announce_auth
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_announce_transfer Module Documentation: announce_transfer Application 
 *  \section Readme_announce_transfer Readme file
 *  \verbinclude Readme.announce_transfer
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_announcement Module Documentation: announcement Application 
 *  \section Readme_announcement Readme file
 *  \verbinclude Readme.announcement
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_conference Module Documentation: conference Application 
 *  \section Readme_conference Readme file
 *  \verbinclude Readme.conference
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_early_announce Module Documentation: early_announce Application 
 *  \section Readme_early_announce Readme file
 *  \verbinclude Readme.early_announce
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_voicemail Module Documentation: voicemail Application 
 *  \section Readme_voicemail Readme file
 *  \verbinclude Readme.voicemail
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_mailbox Module Documentation: mailbox Application 
 *  \section Readme_mailbox Readme file
 *  \verbinclude Readme.mailbox
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_ivr Module Documentation: ivr Application 
 *  \section Readme_ivr Readme file
 *  \verbinclude Readme.ivr
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_py_sems Module Documentation: py_sems Application 
 *  \section Readme_py_sems Readme file
 *  \verbinclude Readme.py_sems
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_reg_agent Module Documentation: reg_agent Application 
 *  \section Readme_reg_agent Readme file
 *  \verbinclude Readme.reg_agent
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_registrar_client Module Documentation: registrar_client Application 
 *  \section Readme_registrar_client Readme file
 *  \verbinclude Readme.registrar_client
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_uac_auth Module Documentation: uac_auth component 
 *  \section Readme_uac_auth Readme file
 *  \verbinclude Readme.uac_auth
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_registrar_client Module Documentation: registrar_client component 
 *  \section Readme_registrar_client Readme file
 *  \verbinclude Readme.registrar_client
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_mp3plugin Module Documentation: mp3 file writer audio plugin
 *  \section Readme_mp3plugin Readme file
 *  \verbinclude Readme.mp3plugin
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_iLBC Module Documentation: iLBC codec plugin
 *  \section Readme_iLBC Readme file
 *  \verbinclude Readme.iLBC
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_callback Module Documentation: callback application plugin
 *  \section Readme_callback Readme file
 *  \verbinclude Readme.callback
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_sw_prepaid_sip Module Documentation: prepaid_sip application plugin
 *  \section Readme_prepaid_sip Readme file
 *  \verbinclude Readme.sw_prepaid_sip
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_click2dial Module Documentation: click2dial application plugin
 *  \section Readme_click2dial Readme file
 *  \verbinclude Readme.click2dial
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */


// -------------------- example apps

/*! \page ModuleDoc_conf_auth Module Documentation: conf_auth Application
 *  \section Readme_conf_auth Readme file
 *  \verbinclude Readme.conf_auth
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_pin_collect Module Documentation: pin_collect Application 
 *  \section Readme_pin_collect Readme file
 *  \verbinclude Readme.pin_collect
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_cacheannounce Module Documentation: cacheannounce Application 
 *  \section Readme_cacheannounce Readme file
 *  \verbinclude Readme.cacheannounce
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_db_announce Module Documentation: db_announce Application 
 *  \section Readme_db_announce Readme file
 *  \verbinclude Readme.db_announce
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_di_dialer Module Documentation: di_dialer Application 
 *  \section Readme_di_dialer Readme file
 *  \verbinclude Readme.di_dial
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_di_log Module Documentation: di_log Application 
 *  \section Readme_di_log Readme file
 *  \verbinclude Readme.di_log
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_jukecall Module Documentation: jukecall Application 
 *  \section Readme_jukecall Readme file
 *  \verbinclude Readme.jukecall
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_mixin_announce Module Documentation: mixin_announce Application 
 *  \section Readme_mixin_announce Readme file
 *  \verbinclude Readme.mixin_announce
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_pinauthconference Module Documentation: pinauthconference example Application 
 *  \section Readme_pinauthconference Readme file
 *  \verbinclude Readme.pinauthconference
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_serviceline Module Documentation: serviceline example Application 
 *  \section Readme_serviceline Readme file
 *  \verbinclude Readme.serviceline
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_early_record Module Documentation: early_record example Application 
 *  \section Readme_early_record Readme file
 *  \verbinclude Readme.earlyrecord
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_simple_conference Module Documentation: simple_conference example Application 
 *  \section Readme_simple_conference Readme file
 *  \verbinclude Readme.simple_conference
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_xmlrpc2di Module Documentation: xmlrpc2di example Application 
 *  \section Readme_xmlrpc2di Readme file
 *  \verbinclude Readme.xmlrpc2di
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

// -------------------- tutorial  apps

/*! \page ModuleDoc_annc_service Module Documentation: annc_service example Application 
 *  \section Readme_annc_service Readme file
 *  \verbinclude Readme.annc_service
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_cc_acc Module Documentation: cc_acc example Application 
 *  \section Readme_cc_acc Readme file
 *  \verbinclude Readme.cc_acc
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_cc_acc_xmlrpc Module Documentation: cc_acc_xmlrpc example Application 
 *  \section Readme_cc_acc_xmlrpc Readme file
 *  \verbinclude Readme.cc_acc_xmlrpc
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_ivr_announce Module Documentation: ivr_announce example Application 
 *  \section Readme_ivr_announce Readme file
 *  \verbinclude Readme.ivr_announce
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_myannounceapp Module Documentation: myannounceapp example Application 
 *  \section Readme_myannounceapp Readme file
 *  \verbinclude Readme.myannounceapp
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_myapp Module Documentation: myapp example Application 
 *  \section Readme_myapp Readme file
 *  \verbinclude Readme.myapp
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_mycc Module Documentation: mycc example Application 
 *  \section Readme_mycc Readme file
 *  \verbinclude Readme.mycc
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_myconfigurableapp Module Documentation: myconfigurableapp example Application 
 *  \section Readme_myconfigurableapp Readme file
 *  \verbinclude Readme.myconfigurableapp
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ModuleDoc_myjukebox Module Documentation: myjukebox example Application 
 *  \section Readme_myjukebox Readme file
 *  \verbinclude Readme.myjukebox
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */

/*! \page ComponentDoc Component Modules Documentation
 *
 *  SEMS is extensible with modules. Component modules are modules which 
 *  implement functionality which can be used by other modules, e.g. by 
 *  application modules. 
 *
 *   <ul><li> \ref ModuleDoc_registrar_client : registrar_client </li></ul>  
 *   <ul><li> \ref ModuleDoc_uac_auth : uac_auth </li></ul>  
 */

