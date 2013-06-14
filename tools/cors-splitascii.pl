#!/usr/bin/env perl
# 
# corsaro
# 
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
# 
# Copyright (C) 2012 The Regents of the University of California.
# 
# This file is part of corsaro.
# 
# corsaro is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# corsaro is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
# 

use strict;

if(@ARGV < 1 || @ARGV > 2)
{
    print STDERR "usage: $0 output_pattern [input_file]\n";
    print STDERR "\twhere output_pattern must include %INTERVAL% to be replaced with\n";
    print STDERR "\tthe interval timestamp\n";
    exit -1;
}

my $output = $ARGV[0];
my $infile = $ARGV[1] ? $ARGV[1] : "-";

unless($output =~ /%INTERVAL%/)
{
    print STDERR "output pattern MUST contain %INTERVAL%\n";
    exit -1;
}

open(INFILE, $infile) or die "could not open $infile";

my $outfile;

while(<INFILE>)
{
    if(/CORSARO_INTERVAL_START/../CORSARO_INTERVAL_END/)
    {
	if(/CORSARO_INTERVAL_START\s+\d+\s+(\d+)/)
	{
	    #switch to a new output file
	    my $time = $1;
	    close $outfile if $outfile;
	    my $this_file = $output;
	    $this_file =~ s/%INTERVAL%/$time/;
	    open($outfile, "> $this_file") or die 
		"could not open interval file $this_file\n";
	}
	print $outfile $_;
    }
}
