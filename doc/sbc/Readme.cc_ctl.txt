CCCtl - control SBC through headers

This call control module exports SBC profile options to be controlled
completely through headers. The module cn be instantiated several times.

Example:
 hdr_ctl.sbcprofile.conf:
  call_control=$H(P-Call-Control)

 Invite incoming:
  INVITE sip:38@192.168.5.110 SIP/2.0
  To: <sip:38@192.168.5.110>
  From: "" <sip:stefan@192.168.5.110>;tag=64015874f26373e6
  ...
  P-Call-Control: ctl;RURI=sip:music@iptel.org
  P-Call-Control: ctl;rtprelay_enabled=yes
  P-Call-Control: ctl;append_headers="P-My-Caller: $fU;param=first"

 Invite outgoing: 
  INVITE sip:music@iptel.org
  To: <sip:38@192.168.5.110>
  From: "" <sip:stefan@192.168.5.110>;tag=64015874f26373e6
  ...
  P-My-Caller: stefan;param=first
  ...

Exported parameters:
    RURI
    From
    To
    Contact
    Call-ID
    outbound_proxy
    force_outbound_proxy  
    next_hop_ip
    next_hop_port
    sst_enabled  ("yes" or "no")
    sst_aleg_enabled  ("yes" or "no")
    append_headers  (incremental)
    rtprelay_enabled ("yes" or "no")
    rtprelay_interface
    aleg_rtprelay_interface
    outbound_interface

    headerfilter   ("transparent", "whitelist", "blacklist")
    header_list    Note: Headers separated by Pipe ('|'), e.g.:
      P-Call-Control: ctl;headerfilter=blacklist;header_list=P-Call-Control|User-Agent

    messagefilter ("transparent", "whitelist", "blacklist")
    message_list   Note: Methods separated by Pipe ('|'), e.g.:
      P-Call-Control: ctl;messagefilter=blacklist;message_list=OPTIONS|MESSAGE
