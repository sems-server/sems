Readme for reg_agent module

This module uses the registrar_client to register SEMS
at a SIP registrar. The accounts  (identities) are set in
the config file.

If the registration is not successful, it tries to 
re-register.

This module depends on the registrar_client module.

Loading uac_auth module is needed if the registration should
be authenticated!
