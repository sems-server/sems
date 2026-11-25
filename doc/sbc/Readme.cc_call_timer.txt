Call timer call control module
------------------------------

This module sets a maximum call duration. The timer is started when the call is
connected; when it expires, the call is ended by the SBC.

Parameters (SBC profile)

timer         - maximum call duration in seconds. If it is not set or empty,
                default_timer is used. With a value of 0, the call is refused
                with 503 Service Unavailable.

Parameters (cc_call_timer.conf)

default_timer - maximum call duration in seconds which is used if 'timer' is
                not set in the SBC profile. If neither of them is set, the
                call is refused with 500.

Configuration example

sbc.conf:
  load_cc_plugins=cc_call_timer

SBC profile (timer taken from P-Timer header):
  call_control=call_timer
  call_timer_module=cc_call_timer
  call_timer_timer=$H(P-Timer)

SBC profile (maximum 90 seconds):
  call_control=call_timer
  call_timer_module=cc_call_timer
  call_timer_timer=90
