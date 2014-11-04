/* \file info about using ZRTP with SEMS
 */

/*!
 * 
 *  \page ZRTP ZRTP encryption  
 *
 *  \section intro Introduction
 * 
 *  <p>ZRTP is a key agreement protocol to negotiate the keys for encryption of RTP in phone calls.
 *  It is an <a href="http://tools.ietf.org/html/rfc6189">informational RFC - RFC6189 ZRTP: Media Path Key Agreement for Unicast Secure RTP</a>.</p>
 *  <p>Even though it uses public key encryption, a PKI is not needed. Since the keys are negotiated 
 *  in the media path, support for it in signaling is not necessary. ZRTP also offers opportunistic 
 *  encryption, which means that calls between UAs that support it are encrypted, but calls to UAs 
 *  not supporting it are still possible, but unencrypted. The actual RTP encryption is done with 
 *  <a href="http://www.ietf.org/rfc/rfc3711.txt">SRTP</a>.
 *
 *  <p>ZRTP is one of the most widely (if not the most widely) supported end-to-end encryption methods for VoIP. 
 *  Popular SIP clients that support ZRTP are <a href="http://www.jitsi.org">Jitsi</a>, 
   <a href="http://www.creytiv.com/baresip.html">baresip</a>, CSipSimple, Twinkle, Linphone.</p>
 * 
 *  <p>For more information about ZRTP, see the 
 *  <a href="http://zfoneproject.com/">Zfone project</a>, the 
 *  <a href="http://tools.ietf.org/html/rfc6189">RFC</a> and the 
 *  <a href="http://en.wikipedia.org/wiki/ZRTP">wikipedia article</a>.</p>
 * 
 *  \section zinsems ZRTP in SEMS
 *  
 *  <p>Since the version 1.0 SEMS supports ZRTP with the use of the
 *   <a href="http://zfoneproject.com/prod_sdk.html"> Zfone SDK</a>.</p>
 *  
 * <p>Currrently, the newest version of the ZRTP SDK, and the one that works with SEMS, is available at 
 * <a href="https://github.com/traviscross/libzrtp">https://github.com/traviscross/libzrtp</a>.
 *
 *  <p>To build SEMS with ZRTP support, install the SDK and set WITH_ZRTP=yes in Makefile.defs,
 *  or build with <br>
 *   <pre> $ make WITH_ZRTP=yes</pre>
 *  </p>
 * 
  <p>ZRTP can be enabled in sems.conf by the enable_zrtp config parameter, e.g. enable_zrtp=yes.</p>

  <p>ZRTP debug logging (lots of info in the log) can be disabled in sems.conf by setting enable_zrtp_debuglog=no</p>

  <p>ZRTP is NOT supported by the sbc application. I.e. if you want to transcrypt cleartext calls into ZRTP encrypted calls,
     you need to use an endpoint application like the webconference module, the conference module, or better yet a
     DSM application. If you want to make a plain-RTP to ZRTP gateway, have a look at the b2b_connect_audio DSM example,
    which can be found in doc/dsm/examples/b2b_connect_audio.
  </p>

  <p> There is support for some utility functions in a DSM module (see \ref dsm_mod_zrtp). 
  </p>

 *  <p>The <em>conference</em> application is enabled to tell the caller the SAS phrase
 *  if it is compiled with WITH_SAS_TTS option, set in apps/conference/Makefile. For this to work,
 *  the <a href="http://cmuflite.org">flite text-to-speech synthesizer</a> version 1.2 or 1.3 is needed.</p>
 *  
 *  \section zinyourapp How to use ZRTP in your application 
 *
 *  Have a look at the dsm module mod_zrtp for examples   
or the conference application on how to add ZRTP support in your application. There is a 
 *  <code>void AmSession::onZRTPEvent(zrtp_event_t event, zrtp_stream_ctx_t *stream_ctx)</code> 
 *  event that is called with the appropriate ZRTP event type and the zrtp stream context, if the state
 *  of the ZRTP encryption changes. The zrtp_event are defined in the Zfone SDK, e.g. ZRTP_EVENT_IS_SECURE.
 *  
 * 
 *  \section zlicense Licensing
 *  
 *  <p>The Zfone SDK is licensed under the Affero GPL v3. As SEMS is licensed under GPL 2+, you may use 
 *  SEMS under GPLv3 and link with libZRTP under Affero GPL v3. You may use the resulting program under
 *  the restrictions of both GPLv3 and AGPLv3.</p>
 * 
 *  <p>Note that due to the nature of the GPL, without written consent of the authors of SEMS as with any other 
 *  non-free library, it is not possible to distribute SEMS linked to specially licensed commercial version of 
 *  the libZRTP SDK, nor the AGPL version. If in doubt, talk to your lawyer.</p>
 *  
 *  \section zphones Phones with ZRTP 
 *  
 *   - <a href="http://zfoneproject.com/">Zfone</a> turns every softphone into a secure phone 
 *      by tapping into the RTP sent and received</li></ul>
 *   - <a href="http://www.jitsi.org">Jitsi</a>
 *   - <a href="http://twinklephone.com/">Twinkle</a> is a very good free softphone for Linux. 
 *     It can speak ZRTP with the use of  GNU 
 *     <a href="http://www.gnutelephony.org/index.php/GNU_ZRTP">libzrtpcpp</a>.
 *
 */
