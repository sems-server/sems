
# diagrams (DSM descriptions)
diag_path=${SEMS_EXEC_PREFIX}/${SEMS_LIBDIR}/sems/dsm/
load_diags=inbound_call,outbound_call

# register these diagrams as 'application' in SEMS
# so it can be used in application= in sems.conf
#register_apps=inbound_call,outbound_call

# for import(mod_name)
mod_path=${SEMS_EXEC_PREFIX}/${SEMS_LIBDIR}/sems/dsm/

# preload modules which have a preload (init) function
#preload_mods=uri

# print raw DSM text while loading to debug log?
# debug_raw_dsm=yes

# do rough consistency checking after loading DSM?
#  (default: yes)
#dsm_consistency_check=no

# DSM to start for in/outbound call if application to execute=dsm
# (from application=xyz in sems.conf, either application=dsm or
#  application=$(apphdr)/$(ruriparam) etc)
#
# use $(mon_select) to search for call info from monitoring
#  e.g. inbound_start_diag=$(mon_select) and see
#  below monitor_select options
#
inbound_start_diag=inbound_call
outbound_start_diag=outbound_call

# prompts files (for prompt collection
load_prompts=${SEMS_CFG_PREFIX}/etc/sems/etc/dsm_in_prompts.conf,${SEMS_CFG_PREFIX}/etc/sems/etc/dsm_out_prompts.conf

# prompts which must be loaded (or else...)
#required_prompts=welcome,error

#load prompt sets config from this path
#prompts_sets_path=${SEMS_CFG_PREFIX}/etc/sems/etc/

# load files with this name (+.conf), e.g. mydomain.conf
#load_prompts_sets=mydomain

# if run_invite_event is set to "yes", an "invite" event is run
# before the session is started. can be used e.g. for early media
# of delaying final reply
#
#run_invite_event=yes

# set_param_variables controls whether application parameters
# from the P-App-Param header are set as variables in the DSM
# dialog. Default: no
#
#set_param_variables=yes

# run these system DSMs on startup (system DSMs are DSMs executed without a call)
#
#run_system_dsms=system_dsm1,system_dsm2

# monitoring_full_stategraph=[yes|no]
#
# Controls whether to log the full call graph (all states visited)
# to the monitoring record. Note this may take some performance
# and use some memory.
#
# Default: no
#
#monitoring_full_stategraph=yes

# monitoring_full_transitions=[yes|no]
#
# Controls whether to log the full call graph (all transitions)
# to the monitoring record. Note this may take some performance
# and use some memory.
#
# Default: no
#
#monitoring_full_transitions=yes

# monitor_select_use_caller=[no|from|pai]
#
# for $(mon_select) application selection from monitoring:
#  select application by caller?
#   from: caller==remote_uri.user (From URI user part)
#   pai:  caller==P-Asserted-Identity user part
#   no:   caller not regarded
#
# Default: from
#
#monitor_select_use_caller=no

# monitor_select_use_callee=[no|ruri|to]
#
# for $(mon_select) application selection from monitoring:
#  select application by callee?
#   ruri: callee==r_uri.user (request URI user part)
#   to:   callee==local_uri.user (To URI user part)
#   no:   callee not regarded
#
# Default: ruri
#
#monitor_select_use_callee=no

# monitor_select_filters=<comma-separated list of filter variables>
#
# for mon_select, records are also filtered by these variables,
# the values taked of P-App-Params. 
#
# e.g. if 
#    P-App-Param: product_id=2;appdomain=iptel.org
# and
#    monitor_select_filters=product_id,appdomain
#  the record in monitoring must have also those avps set:
#   appdomain=iptel.org
#   product_id=2
#
# monitor_select_filters=product_id,appdomain

# monitor_select_fallback=app
#
# fallback application for $(mon_select) application selection
# if record not found for filter, use this app
#
# monitor_select_fallback=default_inbound_app

# conf_dir=<path>
#
# <path> is scanned for *.conf files. Every .conf
# file is loaded, and in the .conf file, the following
# DSM config parameters are processed:
#   diag_path=
#   load_diags=
#   register_apps=
#   mod_path=
#   preload_mods=
#   run_invite_event=
#   set_param_variables=
#   run_system_dsms=
  
#   and additional configuration variables (as script variables)
#
# conf_dir=${SEMS_CFG_PREFIX}/etc/sems/etc/dsm/


###############################################################
# RFC4028 Session Timer
#

# - enables the session timer ([yes,no]; default: no)
#
# enable_session_timer=yes

# - set the "Session-Expires" parameter for the session timer.
#
# session_expires=240

# - set the "Min-SE" parameter for the session timer.
#
# minimum_timer=90

# session refresh (Session Timer, RFC4028) method
#
# INVITE                 - use re-INVITE
# UPDATE                 - use UPDATE
# UPDATE_FALLBACK_INVITE - use UPDATE if indicated in Allow, re-INVITE otherwise
#
# Default: UPDATE_FALLBACK_INVITE
#
# Note: Session Timers are only supported in some applications
#
#session_refresh_method=UPDATE

# accept_501_reply - accept 501 reply as successful refresh? [yes|no]
#
# Default: yes
#
#accept_501_reply=no
