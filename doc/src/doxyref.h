/* \file This file generates Doxygen pages from files in the /doc
 *  directory, and from the readme files in the apps/examples dir.
 */

/*! \page index SEMS Documentation 
 *  \section news News & Changes
 *  \arg \ref changelog 
 *
 *  \section general General
 *  \arg \ref Readme
 *  \arg <a href="http://ftp.iptel.org/pub/sems/sayer_sems_mar10.pdf">VoIP services with SEMS - presentation slides</a>
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
 *  \arg \ref ModuleDoc_sbc
 *  \arg \ref ZRTP
 *  \arg \ref signalsdoc
 *  \arg \ref Tuning
 *
 *  \section developerdoc Developer's documentation
 *   \arg <a href="http://www.iptel.org/files/semsng-designoverview.pdf">
 *         SEMS Design Overview</a>
 *   \arg <a href="http://www.iptel.org/files/semsng-app_module_tutorial.pdf">
 *     Application development tutorial</a> - find the sources in apps/examples/tutorial
 *   \arg \ref AppDocExample
 *   \arg \ref ComponentDoc
 *
 *  \section weblinks Web sites
 *    \arg \b Main:  SEMS website http://iptel.org/sems
 *    \arg \b sems & semsdev Lists: List server http://lists.iptel.org
 *    \arg \b Bugs:  Bug tracker: http://bugtracker.iptel.org/sems
 * 
 *  \section outdated_doc Outdated documentation bits
 *  \arg installation instructions for tutorial applications: 
 *     <a href="http://www.iptel.org/sems/sems_application_development_tutorial"> 
 *    Application Development Tutorial </a>
 *
 */

/*! \page changelog Changelog
 * CHANGELOG for SEMS from 0.9.0 onwards 
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
 * <p>SEMS normally uses one thread per session (processing of the signaling). This thread
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
  <p> You can compile SEMS with thread pool support (see Makefile.defs). This improves 
  performance a lot for high CPS applications, e.g. signaling only B2BUA apps. You should NOT use threadpool if your applications use operations which could be blocking for a longer time (e.g. sleep, remote server access which could possibly be non-responsive), because one blocked session (call) is blocking all the other sessions (calls) that are processed by the same thread. So, for example, if the application logic of one call queries a server synchronously which takes a few seconds to respond, all the other calls are blocked from processing SIP messages and application logic during that time.

The reasons a thread pool gives a large performance boost over one-thread-per-call for high CPS applications presumably are that thread creation takes some time, and the thread scheduling is less efficient if there are very many active threads (as opposed to many sleeping threads like in usual media server applications, where the application/signaling threads sleep most of the time, while only the media/RTP processing threads are active).

On top of that, you save lots of memory (mostly the stack memory), also, because of STL memory allocator.
</p>
  <p> 
 If you notice retransmissions or even failing calls, but the CPU load is not at 100%, there may be several reasons for it:
    - SIP messages are dropped when sending, because the NIC/network is not 
      fast enough in accepting all the packets written to its queue and putting 
      them on the network (you can check this with your OS, for newer Linux in 
      /proc, check dropped packets on send for the SIP port)
    - there is contention on some mutexes
      -> adapt EVENT_DISPATCHER_POWER in AmEventDispatcher.h
      -> add striping for some other Mutexes
  </p>
 */






