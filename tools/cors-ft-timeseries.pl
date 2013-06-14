#!/usr/bin/env perl

use strict;

my $sum; 
my $time;
while(<>)
{
    if(/CORSARO_INTERVAL_START/../CORSARO_INTERVAL_END/)
    {
	if(/CORSARO_INTERVAL_START\s+(\d+)\s+(\d+)/)
	{
	    $time = $2;
	    $sum = 0;
	}
	elsif(/CORSARO_INTERVAL_END/)
	{
	    print "$time,$sum\n";
	}
	else
	{
	    my ($tuple, $val) = split(/,/);
	    $sum+=$val;
	}
    }
}
