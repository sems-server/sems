
/*! \page ModuleDoc_sbc SBC (flexible B2BUA) Application 
 *  \section Readme_sbc Readme file
 *  \verbinclude Readme.sbc.txt
 *  
 *  \section Links
 \subsection sbc_example_profiles Example profiles
 <ul>
<li> \ref sbc_profile_transparent </li>
<li> \ref sbc_profile_auth_b2b </li>
<li> \ref sbc_profile_sst_b2b </li>
<li> \ref sbc_profile_call_timer </li>
<li> \ref sbc_profile_prepaid </li>
<li> \ref sbc_profile_codecfilter </li>
<li> \ref sbc_profile_replytranslate </li>
<li> \ref sbc_profile_refuse </li>
<li> \ref sbc_profile_symmetricrtp </li>
</ul>

 *  Back to \ref AppDoc, to \ref AppDocExample.
 */



/*! \page sbc_profile_transparent SBC example profile: transparent (all profile options)

  This sbc profile has all profile options.

   \section transparent_sbcprofile transparent.sbcprofile.conf
   \verbinclude transparent.sbcprofile.conf
   
   \section Links
   Back to \ref ModuleDoc_sbc, \ref AppDoc, to \ref AppDocExample.
 */

/*! \page sbc_profile_auth_b2b SBC example profile: auth_b2b

  This sbc profile implements the auth_b2b application, an
  identity change with SIP authentication.

   \section auth_b2b_sbcprofile auth_b2b.sbcprofile.conf
   \verbinclude auth_b2b.sbcprofile.conf
   
   \section Links
   Back to \ref ModuleDoc_sbc, \ref AppDoc, to \ref AppDocExample.
 */

/*! \page sbc_profile_sst_b2b SBC example profile: sst_b2b

  This sbc profile implements the sst_b2b application, a
  transparent B2BUA with SIP Session Timer.

   \section sst_b2b_sbcprofile sst_b2b.sbcprofile.conf
   \verbinclude sst_b2b.sbcprofile.conf
   
   \section Links
   Back to \ref ModuleDoc_sbc, \ref AppDoc, to \ref AppDocExample.
 */

/*! \page sbc_profile_call_timer SBC example profile: call_timer

  This sbc profile implements the call_timer application, a
  call timer.

   \section call_timer_sbcprofile call_timer.sbcprofile.conf
   \verbinclude call_timer.sbcprofile.conf
   
   \section Links
   Back to \ref ModuleDoc_sbc, \ref AppDoc, to \ref AppDocExample.
 */

/*! \page sbc_profile_prepaid SBC example profile: prepaid

  This sbc profile implements the sw_prepaid_sip application, a
  prepaid billing B2BUA with interface to separate prepaid module.

   \section prepaid_sbcprofile prepaid.sbcprofile.conf
   \verbinclude prepaid.sbcprofile.conf
   
   \section Links
   Back to \ref ModuleDoc_sbc, \ref AppDoc, to \ref AppDocExample.
 */


/*! \page sbc_profile_codecfilter SBC example profile: codecfilter

  This sbc profile implements a codec filter, it permits only specified
  codecs.

   \section codecfilter_sbcprofile codecfilter.sbcprofile.conf
   \verbinclude codecfilter.sbcprofile.conf
   
   \section Links
   Back to \ref ModuleDoc_sbc, \ref AppDoc, to \ref AppDocExample.
 */

/*! \page sbc_profile_replytranslate SBC example profile: replytranslate

  This sbc profile implements translation of SIP replies.

   \section replytranslate_sbcprofile replytranslate.sbcprofile.conf
   \verbinclude replytranslate.sbcprofile.conf
   
   \section Links
   Back to \ref ModuleDoc_sbc, \ref AppDoc, to \ref AppDocExample.
 */

/*! \page sbc_profile_refuse SBC example profile: refuse

  This sbc profile just refuses calls with a certain code.

   \section refuse_sbcprofile refuse.sbcprofile.conf
   \verbinclude refuse.sbcprofile.conf
   
   \section Links
   Back to \ref ModuleDoc_sbc, \ref AppDoc, to \ref AppDocExample.
 */


/*! \page sbc_profile_symmetricrtp SBC example profile: Symmetric RTP

  This sbc profile implements RTP relay with forced symmetric RTP.

   \section symmetricrtp_sbcprofile symmetricrtp.sbcprofile.conf
   \verbinclude symmetricrtp.sbcprofile.conf
   
   \section Links
   Back to \ref ModuleDoc_sbc, \ref AppDoc, to \ref AppDocExample.
 */

