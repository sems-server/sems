COREPATH =../..

plug_in_name = stats
QUERY_STATS_NAME = sems-stats

module_ldflags = 
module_cflags  = 

extra_target = query_stats
extra_install = install_query_stats
include ../Makefile.app_module

query_stats: query_stats.c Makefile
	$(CXX) $(CXXFLAGS) -o $(QUERY_STATS_NAME) query_stats.c

install_query_stats: query_stats $(DESTDIR)$(bin-prefix)/$(bin-dir)
	$(INSTALL-TOUCH) $(DESTDIR)$(bin-prefix)/$(bin-dir)$(QUERY_STATS_NAME)
	$(INSTALL-BIN) $(QUERY_STATS_NAME) $(DESTDIR)$(bin-prefix)/$(bin-dir)
