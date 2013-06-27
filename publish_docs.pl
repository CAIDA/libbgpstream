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

unless (@ARGV == 1 || @ARGV == 2)
{
    print STDERR "usage: $0 [caida|test|dist] [ssh location]\n";
    exit -1;
}

my $type = $ARGV[0];
my $location = $ARGV[1];

my $caida_host = "cider.caida.org";
my $caida_dir = "/home/alistair/private_html/corsaro";

print STDERR "ensuring docs are built...\n";
my $target;
if($type =~ /caida/ || $type =~ /test/)
{
    $target = "doxy-caida";
}

my $make_docs = "cd docs; make clean; make $target; cd ..";
print STDERR "$make_docs\n";
`$make_docs`;

print STDERR "publishing docs...\n";
my $src_dir = "docs/doxygen/html/";
my $dst_dir;

if($type =~ /caida/ || $type =~ /test/)
{
    $dst_dir = "$caida_host:$caida_dir/";
    $src_dir = "docs/doxygen/caida-html/";
}
else
{
    unless($location)
    {
	print STDERR "a location must be specified when using "
	    ."the 'test' and 'dist' types\n";
	exit -1;
    }
    $dst_dir = $ARGV[1];
}

my $rsync = "rsync -av $src_dir $dst_dir";
print STDERR "$rsync\n";
open(RSYNC, "$rsync |") or die "$! ($rsync)\n";

my @files;

while(<RSYNC>)
{
    chomp;
    next if(/building file list/);
    next if(/^\.\/$/);
    next if(/^\s*$/);
    next if(/sent \d+ bytes/);
    next if(/total size is/);
    push @files, "$caida_dir/$_";
}

#foreach my $file (@files)
#{
#    print "$caida_dir/$file\n";
#    `ssh $caida_host wcs add "$caida_dir/$file"`;
#}

if($type =~ /caida/)
{
    local $" = " ";
    
    my $add_files = "ssh $caida_host wcs add @files";
    #print "$add_files\n";
    `$add_files`;
    
    `ssh $caida_host wcs pub "$caida_dir"`;
}
