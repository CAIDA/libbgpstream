#!/usr/bin/env perl
#
# This file is part of bgpstream
#
# Copyright (C) 2015 The Regents of the University of California.
# Authors: Alistair King, Chiara Orsini
#
# All rights reserved.
#
# This code has been developed by CAIDA at UC San Diego.
# For more information, contact bgpstream-info@caida.org
#
# This source code is proprietary to the CAIDA group at UC San Diego and may
# not be redistributed, published or disclosed without prior permission from
# CAIDA.
#
# Report any bugs, questions or comments to bgpstream-info@caida.org
#

use strict;

unless (@ARGV == 1 || @ARGV == 2)
{
    print STDERR "usage: $0 [caida|test] [ssh location]\n";
    exit -1;
}

my $type = $ARGV[0];
my $location = $ARGV[1];

my $caida_host = "cider.caida.org";
my $caida_dir = "public_html/bgpstream";

print STDERR "ensuring docs are built...\n";
my $make_docs = "cd docs; make clean; make; cd ..";
print STDERR "$make_docs\n";
`$make_docs`;

print STDERR "publishing docs...\n";
my $src_dir = "docs/doxygen/html/";
my $dst_dir = "$caida_host:$caida_dir/";

my $rsync = "rsync --delete -av $src_dir $dst_dir";
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
