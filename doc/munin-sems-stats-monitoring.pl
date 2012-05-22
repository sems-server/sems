#!/usr/bin/perl

if($ARGV[0] and $ARGV[0] eq 'config') {
        print "graph_title SEMS calls\n";
        print "graph_args --base 1000 -l 0\n";
        print "graph_vlabel calls\n";
        print "graph_category Porting\n";

	print "calls.draw LINE2\n";
	print "calls.label current\n";
	print "peak.draw LINE2\n";
	print "peak.label peak calls\n";
        exit 0;
}

open(FILE, "sems-stats|");

while(<FILE>)
{
	if($_ =~ /Active calls: (.*)\n/)
	{
		print "calls.value $1\n";
	}
}
close FILE;

open(FILE, "sems-stats -c get_callsmax|");

while(<FILE>)
{
	if($_ =~ /Maximum active calls: (.*)\n/)
	{
		print "peak.value $1\n";
	}
}
close FILE;
