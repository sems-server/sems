Sample SBC confguration with two interfaces and registration caching

SEMS listening at 192.168.6.110:5060 ("external interface")
SEMS listening at 192.168.5.110:5080 ("internal interface")
Registrar at 192.168.5.110:5060

clients registering to username@192.168.6.110

SEMS is setting the RURI domain towards the registrar to 192.168.5.110, could be removed
if the registrar accepts DNS name.

setup & try in-tree e.g. like this:

git clone https://github.com/sems-server/sems.git
cd sems; make; cd core
./sems -f ../doc/sbc/sample_config_regcache/sems.conf