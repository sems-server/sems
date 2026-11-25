Syslog CDR call control module
------------------------------

This module writes a CDR for every call to syslog(3) when the call ends. The
CDR is one line of comma-separated values (CSV), which can be processed by
standard syslog utils, e.g. syslog-ng.

All values that are configured for the call control section in the SBC profile
are written to the CDR, and can contain the usual replacement patterns.

Parameters (cc_syslog_cdr.conf)

cdr_prefix      - prefix to place before every CDR (e.g. to filter out in
                  syslogd.conf); default: "CDR: "

loglevel        - priority to log with:
                    0  LOG_ERR
                    1  LOG_WARNING
                    2  LOG_INFO
                    3  LOG_DEBUG
                    4  LOG_NOTICE
                  default: 2 (LOG_INFO)

quoting_enabled - [yes | no] : enable double quotes around values. If enabled,
                  double quotes in values are doubled, e.g. "Joe" -> """Joe""";
                  default: yes

cdr_format      - comma-separated list of the items to write. Items can be
                    $ltag                            local tag (ID for call)
                    $start_ts, $connect_ts, $end_ts  timestamp (sec.usec since
                                                     epoch)
                    $start_tm, $connect_tm, $end_tm  timestamp (%F %T format)
                    $duration                        start to end (sec.ms)
                    $duration_sec                    start to end (sec)
                    $bill_duration                   connect to end (sec.ms)
                    $bill_duration_sec               connect to end (sec)
                    $setup_duration                  start to connect/end
                                                     (sec.ms)
                    $setup_duration_sec              start to connect/end (sec)
                    $varname, $varname.property      call control variable
                    <name>                           value configured in the
                                                     SBC profile
                  default: $ltag,$start_ts,$connect_ts,$end_ts,<all configured
                  values>

Configuration example

sbc.conf:
  load_cc_plugins=cc_syslog_cdr

cc_syslog_cdr.conf:
  cdr_prefix="SBC-CDR: "
  cdr_format=$ltag,$start_tm,$connect_tm,$end_tm,$bill_duration,Calling-Station-Id,Called-Station-Id

SBC profile:
  call_control=cdr
  cdr_module=cc_syslog_cdr
  cdr_Calling-Station-Id=$fU
  cdr_Called-Station-Id=$tU
  cdr_Sip-From-Tag=$ft
