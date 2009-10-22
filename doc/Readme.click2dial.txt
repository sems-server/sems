#######################################################################
#
# click2dial, an xmlrpc-enabled way to initiate authenticated calls
# Copyright (C) 2007 Sipwise GmbH
# Based on "announcement", Copyright (C) 2002-2003 Fhg Fokus
#
# Author: Andreas Granig <agranig@sipwise.com>
#
########################################################################

How click2dial works
------------------------

Simply issue an xmlrpc request as shown in apps/click2dial/example.py
to initiate an authenticated call.

First, call leg (a) is connected, and an announcemnet defined in
click2dial.conf is played. Then call leg (b) is contacted, and on success
the two call legs are connected.

