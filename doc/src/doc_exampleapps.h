/*! \page AppDocExample Example Applications
 * 
 * These are examples that illustrate how to
 * make use certain aspects of the SEMS framework rather than
 * be complete ready-to-use applications. The examples given
 * here can be a good start for further development, though, and some,
 * like the xmlrpc2di, cacheannounce, db_announce, serviceline and the
 * calling card application might be used right out of the box.
 * 
 *
 * \section exampleappb2bapps Back-to-back user agent applications
 * 
 * The b2b_connect is a transparent back-to-back user agent application, 
 * that optionally authenticates the second call leg. If can be used as kind of 
 * simple SBC and for other such purposes. 
 * 
 * \ref ModuleDoc_b2b_connect
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
 * call_gen (\ref ModuleDoc_call_gen) is a call generator, e.g. a load generator for testing SEMS.
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
 * The py_sems_ex directory contains some example for the py_sems embedded Python interpreter, namely 
 * an early media announcement implementation, and the jukecall example as py_sems application.
 *
 * The early_record (\ref ModuleDoc_early_record) is an example on how to receive and use early media. 
 * 
 * dtmftester (\ref ModuleDoc_dtmftester) just records the file and plays the digits, to check whether 
 * DTMF detection is working.
 * 
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

/*! \page ModuleDoc_call_gen Module Documentation: call_gen Application 
 *  \section Readme_call_gen Readme file
 *  \verbinclude Readme.call_gen
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

/*! \page ModuleDoc_dtmftester Module Documentation: dtmftester example Application 
 *  \section Readme_dtmftester Readme file
 *  \verbinclude Readme.dtmftester
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

/*! \page ModuleDoc_b2b_connect Module Documentation: b2b_connect example Application 
 *  \section Readme_b2b_connect Readme file
 *  \verbinclude Readme.b2b_connect
 *  
 *  \section Links
 *  Back to \ref AppDoc, to \ref AppDocExample.
 */
 
