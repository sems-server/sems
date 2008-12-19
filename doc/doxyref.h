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
 *  \arg \ref ZRTP
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





