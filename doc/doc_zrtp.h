/* \file info about ZRTP usage in SEMS
 */

/*!
 * 
 *  \page ZRTP ZRTP encryption  
 *
 *  \section intro Introduction
 * 
 *  <p>ZRTP is a key agreement protocol to negotiate the keys for encryption of RTP in phone calls.
 *  It is a proposed public standard: <a href="http://tools.ietf.org/html/draft-zimmermann-avt-zrtp"> 
 *  ZRTP: Media Path Key Agreement for Secure RTP</a>.</p>
 *  <p>Even though it uses public key encryption, a PKI is not needed. Since the keys are negotiated 
 *  in the media path, support for it in signaling is not necessary. ZRTP also offers opportunistic 
 *  encryption, which means that calls between UAs that support it are encrypted, but calls to UAs 
 *  not supporting it are still possible, but unencrypted. The actual RTP encryption is done with 
 *  <a href="http://www.ietf.org/rfc/rfc3711.txt">SRTP</a>.
 *  For more information about ZRTP, see the 
 *  <a href="http://zfoneproject.com/">Zfone project</a>, the 
 *  <a href="http://tools.ietf.org/html/draft-zimmermann-avt-zrtp">draft</a> and the 
 *  <a href="http://en.wikipedia.org/wiki/ZRTP">wikipedia article</a>.</p>
 * 
 *  \section zinsems ZRTP in SEMS
 *  
 *  <p>Since the version 1.0 SEMS supports ZRTP with the use of the
 *   <a href="http://zfoneproject.com/prod_sdk.html"> Zfone SDK</a>.</p>
 *  
 *  <p>To build SEMS with ZRTP support, install the SDK and set WITH_ZRTP=yes in Makefile.defs,
 *  or build with <br>
 *   <pre> $ make WITH_ZRTP=yes</pre>
 *  </p>
 * 
 *  <p>The <em>conference</em> application is enabled to tell the caller the SAS phrase
 *  if it is compiled with WITH_SAS_TTS option, set in apps/conference/Makefile. For this to work,
 *  the <a href="http://cmuflite.org">flite text-to-speech synthesizer</a> version 1.2 or 1.3 is needed.</p>
 *  
 *  \section onlinedemo Online demo
 * 
 *  <p>Call <pre>sip:secureconference@iptel.org</pre> or <pre>sip:zrtp@iptel.org</pre> for a test drive 
 *  of ZRTP conferencing. If you call that number with a ZRTP enabled phone, you should be told the SAS string
 *  that is also displayed in your phone. Press two times the hash (##) while in the call to read out the 
 *  SAS string again.</p>
 *
 *  \section zinyourapp How to use ZRTP in your application 
 *
 *  Have a look at the conference application on how to add ZRTP support in your application. There is a 
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
 *   - <a href="http://twinklephone.com/">Twinkle</a> is a very good free softphone for Linux. 
 *     It can speak ZRTP with the use of  GNU 
 *     <a href="http://www.gnutelephony.org/index.php/GNU_ZRTP">libzrtpcpp</a>.
 *
 */
