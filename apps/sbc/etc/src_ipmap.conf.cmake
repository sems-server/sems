# this is a sample regex mapping to map source address 10.0.* to internal1 profile
# and 10.1.* to internal2 profile.
# For example, used with this active_profile setting: 
#    active_profile=$M($si=>src_ipmap),refuse
# all other calls would be blocked.
^10\.0\..*=>internal1
^10\.1\..*=>internal2
