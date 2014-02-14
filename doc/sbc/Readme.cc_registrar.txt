Registrar call control module
-----------------------------

This module makes SEMS behave as registrar. It processes REGISTER requests saving
the contact in the local register cache, and routes other requests by looking up
the location from the local register cache.

Todo: lots of things
 - caching + pass through mode
 - authentication (separate cc?)
 
Parameters

nat_handling  - [yes | no] : NAT handling, i.e. send request to IP:port where
                             REGISTER was received from; default: yes
sticky_iface  - [yes | no] : use interface that REGISTER was received from for
                             routing request; default: yes

                             
Configuration example
---------------------

 sbc.conf
 --------
 load_cc_plugins=cc_registrar
 profiles=registrar
 active_profile=registrar

 registrar.sbcprofile.conf
 -------------------------
 call_control=registrar
 registrar_module=cc_registrar
 registrar_nat_handling=yes
 registrar_sticky_iface=yes

