/* \file This file generates Doxygen pages from files in the /doc
 *  directory, and from the readme files in the apps/examples dir.
 */

/*! \page index SEMS Documentation 
 *  \section news News & Changes
 *  \arg \ref changelog 
 *
 *  \section general General
 *  \arg \ref Readme 
 * 
 *  \section howtostart How to get started
 *  \arg \ref howtostart_noproxy
 *  \arg \ref howtostart_simpleproxy
 *  \arg \ref howtostart_voicemail
 *
 *  \section userdoc User's documentation
 *  \arg \ref sems.conf.sample
 *  \arg \ref Compiling 
 *  \arg \ref AppDoc
 *  \arg \ref ModuleDoc_dsm
 *  \arg \ref ZRTP
 *  \arg \ref Tuning
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
 *  \section outdated_doc Outdated documentation bits
 *  \arg \ref whatsnew_0100 
 *  \arg \ref Configure-Sems-Ser-HOWTO
 *
 */

/*! \page whatsnew_0100 Changes in SEMS from 0.9 versions to 0.10 
 * SEMS has changed a lot between 0.9 versions and 0.10 versions.
 * The file WHATSNEW_0.10 lists the most important changes:
 * \verbinclude WHATSNEW_0.10
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
 * <p><b>
 * This is only needed if SER, the SIP Express Router, is used as SIP stack for SEMS. Since 1.0 
 * you can simply use the sipctrl module, which provides a SIP stack for SEMS. sipctrl is set
 * as the default control plugin, so usually there need not be any changes in that regard.</p>
 * <p>But, if you need more functionality from the SIP stack, e.g. TLS, TCP, filtering of inbound 
 * requests etc., you can use ser2 with the binrpcctl module, and the SASI SER application server 
 * interface, and then parts of the information below apply.
 * </p></b>
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

/*! \page Tuning Tuning SEMS for high load
 * 
 * <p>For high load, there are several compile and run time options 
 * to make SEMS run smoothly.</p>
 * 
 * <p>When running SEMS, make sure that you have the ulimit for open files 
 * (process.max-file-descriptor) set to an value which is high enough. 
 * You may need to adapt raise the system wide hard limit (on Linux see 
 * /etc/security/limits.conf), or run SEMS as super
 * user. Note that an unlimited open files limit is not possible, but it is sufficient
 * to set it to some very high value (e.g. ulimit -n 100000).</p>
 * 
 * <p>There is a compile-time variable that sets a limit on how many RTP sessions are 
 * supported concurrently, this is MAX_RTP_SESSIONS. You may either add this at compile
 * time to your value, or edit Makefile.defs and adapt the value there.</p>
 * 
 * <p>SEMS uses one thread per session (processing of the signaling). This thread
 * sleeps on a mutex (the session's event queue) most of the time 
 * (RTP/audio processing is handled by the AmMediaProcessor
 * threads, which is only a small, configurable, number), thus the scheduler should 
 * usually not have any performance issue with this. The advantage of using a 
 * thread per call/session
 * is that if the thread blocks due to some blocking operation (DB, file etc), processing 
 * of other calls is not affected. The downside of using a thread per session is that you 
 * will spend memory for the stack for every thread, which can fill up your system memory 
 * quickly, if you have many sessions. The default for the stack size is 1M, which for most
 * cases is quite a lot, so if memory consumption is an issue, you could adapt this 
 * in AmThread, at the call to pthread_attr_setstacksize. Note that, at least in Linux, 
 * the memory is allocated, but if a page is not used, the page is not really consumed, which
 * means that most of that empty memory space for the stack is not really consumed anyway. 
 * If you allocate more than system memory for stack, though, thread creation may still fail
 * with ENOMEM.</p>
 */






