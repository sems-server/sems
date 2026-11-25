Parallel calls call control module
----------------------------------

This module limits the number of parallel (concurrent) calls. The calls are
counted per "uuid", which can be any string, e.g. the caller user, the source IP
address or a value taken from a header. If the limit is reached, further calls
are refused.

Parameters (SBC profile)

uuid          - identifier for which calls are counted (mandatory)
max_calls     - maximum number of parallel calls for the uuid; default: 1.
                With max_calls=0 all calls are refused.

Parameters (cc_pcalls.conf)

refuse_code   - code to refuse calls over the limit with; default: 402
refuse_reason - reason to refuse calls over the limit with;
                default: "Too Many Simultaneous Calls"

Configuration example

sbc.conf:
  load_cc_plugins=cc_pcalls

SBC profile (limit From-User to max 5 calls):
  call_control=pcalls
  pcalls_module=cc_pcalls
  pcalls_uuid=$fU
  pcalls_max_calls=5
