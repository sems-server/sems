/*! \page signalsdoc SEMS signal handling

\section sigtitle How signals are handled by SEMS

The execution of the server can be controlled by sending signals
to the process.

 \li  SIGHUP - broadcast shutdown, usually sends BYE and stops all calls. The server does not terminate.
 \li  SIGTERM, SIGINT - broadcast shutdown (usually sends BYE and stops all calls), and stop.
 \li  SIGUSR1, SIGUSR2 - broadcast a system event with id User1 or User2, can for example be used in DSM with system(#type=="User2") condition

\section see_also See also

 \li max_shutdown_time in sems.conf:  \ref sems.conf.sample


 */
