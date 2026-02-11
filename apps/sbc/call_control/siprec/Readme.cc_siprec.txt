cc_siprec - SIPREC (RFC 7865/7866) Session Recording Client

Enables automatic call recording by forking RTP from both call legs
to a Session Recording Server (SRS) via the SIPREC protocol.

======================================================================
 SEMS Configuration
======================================================================

Three configuration files form the chain that enables SIPREC recording:

  sems.conf -> sbc.conf -> <profile>.sbcprofile.conf -> cc_siprec.conf

1. sems.conf - set the SBC as the application:

     application=sbc

   If using load_plugins, make sure sbc is included:

     load_plugins=wav;gsm;sbc

   If using exclude_plugins, make sure sbc is NOT listed.

2. sbc.conf - load the cc_siprec plugin, define and activate a profile:

     load_cc_plugins=cc_siprec
     profiles=myprofile
     active_profile=myprofile

   This loads the cc_siprec call_control module and tells the SBC
   to use myprofile.sbcprofile.conf from the same config directory.

   Multiple profiles can be listed (comma-separated). The active
   profile can be selected dynamically, e.g.:

     active_profile=$(ruri.user)

3. myprofile.sbcprofile.conf - enable siprec in the profile:

     call_control=siprec
     siprec_module=cc_siprec
     siprec_enable=yes

   Set siprec_enable=no to disable recording for this profile
   without unloading the module. Default is yes (enabled).

======================================================================
 cc_siprec Module Configuration
======================================================================

Configure the module in /etc/sems/etc/cc_siprec.conf:

     srs_uri=sip:recorder@<SRS_IP>:5060
     recording_mandatory=no
     codec=PCMA

     # RTP transport profile (RFC 7866 Section 12.2)
     # RTP/AVP (default), RTP/SAVP, RTP/AVPF, RTP/SAVPF
     rtp_transport=RTP/AVP

     # Local RTP port range for SIPREC media streams.
     # Must not overlap with SBC relay ports or siprec_srs ports.
     rtp_port_min=50000
     rtp_port_max=50999

   Supported codecs:
     PCMA = G.711 A-law (payload type 8, 8000 Hz) - default
     PCMU = G.711 u-law (payload type 0, 8000 Hz)

   The codec is auto-detected from the Communication Session SDP
   when available (the first offered audio codec is used). The
   configured codec serves as a fallback when SDP extraction fails.

======================================================================
 RFC 7865/7866 Compliance Notes
======================================================================

The following RFC requirements are implemented:

  SIP Signaling:
  - Require: siprec option tag in RS INVITE (RFC 7866 Section 6.1.1)
  - +sip.src feature tag in Contact URI (RFC 7866 Section 6.1.1)
  - Content-Disposition: recording-session on metadata (RFC 7866 Section 9.1)
  - multipart/mixed body with SDP + metadata XML (RFC 7866 Section 9.1)
  - CANCEL for pending INVITE on call teardown (RFC 3261)
  - re-INVITE for hold/resume with updated SDP + metadata

  SDP:
  - Separate sendonly m-lines per CS direction (RFC 7866 Section 8.3)
  - a=label on each media stream (RFC 7866 Section 7.1.1)
  - Real allocated ports (symmetric RTP - RFC 7866 Section 8.1.8)
  - Configurable RTP profile (AVP/SAVP/AVPF/SAVPF)

  Metadata XML (RFC 7865):
  - Namespace urn:ietf:params:xml:ns:recording:1
  - session, participant, stream elements with base64 IDs
  - sessionrecordingassoc (CS-to-RS association)
  - participantsessionassoc (participant-to-session association)
  - participantstreamassoc with send/recv stream references
  - ISO 8601 (RFC 3339) timestamps
  - Partial metadata updates on hold (dataMode="partial")
  - Final metadata with stop-time in BYE

  Media:
  - RTP forwarded separately per call leg direction
  - RTCP ports allocated (port+1 per RTP convention)
  - Symmetric RTP: forwarder sockets bound to advertised ports

  Recording Indication (RFC 7866 Section 6.1.2):
  - a=record:on injected into B-leg (callee) INVITE SDP
  - A-leg indication requires deployment policy (tone or contract)

  Security (RFC 7866 Section 12):
  - RTP/SAVP and RTP/SAVPF transport profiles configurable
  - Use sips: URI scheme for SRS to enable TLS transport
  - SDES key exchange requires SEMS TLS/SRTP stack configuration

======================================================================
 Testing with siprec_srs (built-in SEMS SRS)
======================================================================

SEMS includes a minimal SIPREC Session Recording Server (siprec_srs)
that can run on the same instance as the SBC. This is the simplest
way to test cc_siprec -- no external software required.

Each recording session produces:
  - {session_id}_leg1.wav   (A-leg / caller audio)
  - {session_id}_leg2.wav   (B-leg / callee audio)
  - {session_id}_initial.xml (SIPREC metadata from INVITE)
  - {session_id}_final.xml   (SIPREC metadata from BYE, if present)

Output format: 16-bit PCM WAV, 8000 Hz, mono.
Supported codecs: PCMA (G.711 A-law) and PCMU (G.711 u-law).

----------------------------------------------------------------------
 Step 1: Configure sems.conf
----------------------------------------------------------------------

Switch to application mapping and load both plugins:

  application=$(mapping)
  load_plugins=sbc;siprec_srs

Make sure the RTP port ranges don't overlap between the SBC, SRS,
and SIPREC SRC:

  rtp_low_port=10000
  rtp_high_port=39999

  (SRS uses 40000-40999, SRC uses 50000-50999 by default)

----------------------------------------------------------------------
 Step 2: Configure app_mapping.conf
----------------------------------------------------------------------

Route the SRS URI to siprec_srs, everything else to the SBC:

  ^sip:siprec_srs@=>siprec_srs
  .*=>sbc

----------------------------------------------------------------------
 Step 3: Configure cc_siprec.conf
----------------------------------------------------------------------

Point to the SRS on localhost:

  srs_uri=sip:siprec_srs@127.0.0.1
  rtp_port_min=50000
  rtp_port_max=50999

----------------------------------------------------------------------
 Step 4: Configure sbc.conf
----------------------------------------------------------------------

Load the cc_siprec plugin:

  load_cc_plugins=cc_siprec

----------------------------------------------------------------------
 Step 5: Configure SBC profile
----------------------------------------------------------------------

In your profile (e.g. siprec.sbcprofile.conf):

  call_control=siprec
  siprec_module=cc_siprec
  siprec_enable=yes

  # RTP relay is REQUIRED for SIPREC recording.
  # cc_siprec hooks into the relay path (onAfterRTPRelay) to fork
  # RTP packets to the SRS. Without relay mode, no audio is captured.
  enable_rtprelay=yes
  rtprelay_transparent_seqno=yes
  rtprelay_transparent_ssrc=yes

----------------------------------------------------------------------
 Step 6: Configure siprec_srs.conf
----------------------------------------------------------------------

  recording_dir=/var/spool/sems/siprec
  rtp_port_min=40000
  rtp_port_max=40999

The RTP port range must NOT overlap with sems.conf rtp_low_port/
rtp_high_port or cc_siprec.conf rtp_port_min/rtp_port_max.

----------------------------------------------------------------------
 Step 7: Make a test call
----------------------------------------------------------------------

1. Start SEMS.

2. Register Phone A and Phone B through the SEMS SBC.

3. Place a call from Phone A to Phone B. The call connects normally.

4. When the call connects, cc_siprec sends a SIPREC INVITE to
   siprec_srs. Check the SEMS log for:

     SIPREC: sent INVITE to SRS 'sip:siprec_srs@127.0.0.1'
     SIPREC: recording started, forwarding RTP to 127.0.0.1:...

5. Speak on both phones for a few seconds, then hang up.

6. Check SEMS log for:

     SIPREC: sent BYE to SRS

----------------------------------------------------------------------
 Step 8: Verify the recording
----------------------------------------------------------------------

Check the recording directory:

  ls -la /var/spool/sems/siprec/

You should see WAV and XML files for the recorded call.

Play back with:

  aplay /var/spool/sems/siprec/*_leg1.wav
  aplay /var/spool/sems/siprec/*_leg2.wav

----------------------------------------------------------------------
 Troubleshooting
----------------------------------------------------------------------

SEMS log shows "no SRS URI configured":
  - Check that srs_uri is set in cc_siprec.conf
  - Check that the module is listed in call_control= in sbcprofile

SEMS log shows "failed to send INVITE to SRS":
  - Verify siprec_srs is loaded: check sems.conf load_plugins
  - Verify app_mapping routes the SRS URI to siprec_srs

SEMS log shows "failed to allocate RTP ports":
  - Check rtp_port_min/rtp_port_max in cc_siprec.conf
  - Verify the port range is not exhausted or in use

SEMS log shows "bind(...) failed":
  - Port range may overlap with another service
  - Check rtp_port_min/rtp_port_max doesn't conflict with SBC or SRS

Recording WAV files contain 0 bytes:
  - RTP relay must be enabled in the SBC profile (enable_rtprelay=yes)
  - Without relay mode, onAfterRTPRelay never fires and no RTP is forked

Recording contains silence or one-way audio:
  - Verify both RTP forwarders started (check SEMS log for port numbers)
  - Check that the SRS RTP port range doesn't overlap with the SBC range
  - Use tcpdump to confirm RTP arriving on the SRS ports:
    tcpdump -i lo udp portrange 40000-40999 -n
