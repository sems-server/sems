/* \file This file generates Doxygen pages from files in the /doc
 directory 
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
 *  \arg <a href="http://www.iptel.org/howto/sems_application_development_tutorial"> 
 *    Application Development Tutorial </a>
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
 * CHANGELOG for SEMS which is from 0.10.0-rc1 onwards 
 * \verbinclude CHANGELOG
 * 
 */

/*! \page Readme SEMS Readme file
 * SEMS Readme file:
 * 
 * \verbinclude README
 * 
 */

/*! \page Compiling Additional compiling instructions (ivr and mp3)
 * \verbinclude COMPILING
 * 
 */

/*! \page Configure-Sems-Ser-HOWTO mini-Howto on how to configure SER and SEMS to work together
 *
 * Be sure to also check the instructions that are on the page with the 
 * <a href="http://www.iptel.org/howto/sems_application_development_tutorial">
 * application development tutorial</a>, there is step by step instructions including 
 * a complete set of configuration files that should get you started with SEMS 
 * very quickly.
 *
 * \verbinclude Configure-Sems-Ser-HOWTO
 * 
 */

/*! \page sems.conf.sample SEMS core configuration parameters
 * core/sems.conf.sample
 * 
 * \verbinclude sems.conf.sample
 * 
 */

/*! \page AppDoc Application Modules Documentation
 *  Documentation for the applications that come with SEMS. 
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
 *  is the <i>announce_auth</i> application: 
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
 * \section IVR: Python Scripting application 
 *
 * The <i>ivr</i> module plugin embeds a python interpreter into SEMS. In it, 
 * applications written in python can be run (<i>mailbox</i>, <i>conf_auth</i>,
 * <i>pin_collect</i> for example) and new applications can be prototyped and 
 * implemented very quickly: 
 *
 * <ul><li> \ref ModuleDoc_ivr  </li></ul>
 * 
 */

/*! \page ModuleDoc_ann_b2b Module Documentation: ann_b2b Application 
 *  \section Readme_ann_b2b Readme file
 *  \verbinclude Readme.ann_b2b
 */

/*! \page ModuleDoc_announce_auth Module Documentation: announce_auth Application 
 *  \section Readme_announce_auth Readme file
 *  \verbinclude Readme.announce_auth
 */

/*! \page ModuleDoc_announce_transfer Module Documentation: announce_transfer Application 
 *  \section Readme_announce_transfer Readme file
 *  \verbinclude Readme.announce_transfer
 */
/*! \page ModuleDoc_ann_b2b Module Documentation: ann_b2b Application 
 *  \section Readme_ann_b2b Readme file
 *  \verbinclude Readme.ann_b2b
 */

/*! \page ModuleDoc_announcement Module Documentation: announcement Application 
 *  \section Readme_announcement Readme file
 *  \verbinclude Readme.announcement
 */

/*! \page ModuleDoc_conference Module Documentation: conference Application 
 *  \section Readme_conference Readme file
 *  \verbinclude Readme.conference
 */

/*! \page ModuleDoc_early_announce Module Documentation: early_announce Application 
 *  \section Readme_early_announce Readme file
 *  \verbinclude Readme.early_announce
 */

/*! \page ModuleDoc_voicemail Module Documentation: voicemail Application 
 *  \section Readme_voicemail Readme file
 *  \verbinclude Readme.voicemail
 */

/*! \page ModuleDoc_mailbox Module Documentation: mailbox Application 
 *  \section Readme_mailbox Readme file
 *  \verbinclude Readme.mailbox
 */

/*! \page ModuleDoc_ivr Module Documentation: ivr Application 
 *  \section Readme_ivr Readme file
 *  \verbinclude Readme.ivr
 */

/*! \page ModuleDoc_uac_auth Module Documentation: uac_auth component 
 *  \section Readme_uac_auth Readme file
 *  \verbinclude Readme.uac_auth
 */

/*! \page ModuleDoc_registrar_client Module Documentation: registrar_client component 
 *  \section Readme_registrar_client Readme file
 *  \verbinclude Readme.registrar_client
 */

/*! \page ModuleDoc_mp3plugin Module Documentation: mp3 file writer audio plugin
 *  \section Readme_mp3plugin Readme file
 *  \verbinclude Readme.mp3plugin
 */

/*! \page ModuleDoc_iLBC Module Documentation: iLBC codec plugin
 *  \section Readme_iLBC Readme file
 *  \verbinclude Readme.iLBC
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

