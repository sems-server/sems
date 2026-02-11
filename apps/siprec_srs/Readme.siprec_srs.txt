siprec_srs - Minimal SIPREC Session Recording Server
=====================================================

A minimal SIPREC SRS (RFC 7865/7866) that runs as a SEMS application.
Receives recording INVITEs from SIPREC clients (such as cc_siprec),
records both call legs to separate WAV files, and saves the SIPREC
metadata XML.

Each recording session produces:
  - {session_id}_leg1.wav   (A-leg / caller audio)
  - {session_id}_leg2.wav   (B-leg / callee audio)
  - {session_id}_initial.xml (SIPREC metadata from INVITE)
  - {session_id}_final.xml   (SIPREC metadata from BYE, if present)

Supported codecs: PCMA (G.711 A-law) and PCMU (G.711 u-law).
Output: 16-bit PCM WAV, 8000 Hz, mono.


Configuration: siprec_srs.conf
------------------------------

  # Directory for recordings (created automatically if missing)
  recording_dir=/var/spool/sems/siprec

  # RTP port range (must NOT overlap with sems.conf rtp_low_port/rtp_high_port)
  rtp_port_min=40000
  rtp_port_max=40999


Self-test setup (SBC + SRS on same SEMS instance)
--------------------------------------------------

This allows cc_siprec to record calls by sending SIPREC INVITEs to
the same SEMS instance running the SRS.

1. sems.conf - switch to application mapping:

     application=$(mapping)
     load_plugins=sbc;siprec_srs

   Make sure the RTP port ranges don't overlap:
     rtp_low_port=10000
     rtp_high_port=39999

2. app_mapping.conf - route SRS URI to siprec_srs, everything else to SBC:

     ^sip:siprec_srs@=>siprec_srs
     .*=>sbc

3. cc_siprec.conf - point to the SRS on localhost:

     srs_uri=sip:siprec_srs@127.0.0.1

4. sbc.conf - load both cc plugins:

     load_cc_plugins=cc_siprec;cc_registrar

5. siprec.sbcprofile.conf - enable recording:

     call_control=siprec,registrar
     siprec_module=cc_siprec
     siprec_enable=yes
     registrar_module=cc_registrar

     # RTP relay is REQUIRED for SIPREC recording.
     # cc_siprec hooks into the relay path (onAfterRTPRelay) to fork
     # RTP packets to the SRS. Without relay mode, no audio is captured.
     enable_rtprelay=yes
     rtprelay_transparent_seqno=yes
     rtprelay_transparent_ssrc=yes

6. Start SEMS and make a test call. Recordings appear in:

     /var/spool/sems/siprec/

   Play back with:
     aplay /var/spool/sems/siprec/*_leg1.wav
     aplay /var/spool/sems/siprec/*_leg2.wav
