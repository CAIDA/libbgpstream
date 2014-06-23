#!/usr/bin/env perl
#
# bgpanalyzer
#
# Chiara Orsini, CAIDA, UC San Diego
# chiara@caida.org
#
# Copyright (C) 2014 The Regents of the University of California.
#
# This file is part of bgpanalyzer.
#
# bgpanalyzer is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# bgpanalyzer is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with bgpanalyzer.  If not, see <http://www.gnu.org/licenses/>.
#

use warnings;
use strict;
use Getopt::Std;    # to process command line options 
use File::Path qw(make_path);   # to create the output folder
use Statistics::Discrete;




my %opts;
my $opt;
my $valid_opts = 0;

my ($input_file, $output_folder);

getopts('i:o:', \%opts);
if(defined $opts{"i"} && defined $opts{"o"}){
  $valid_opts = 1;
  $input_file = $opts{"i"};
  $output_folder = $opts{"o"};
}

if ($valid_opts == 0){
  print "Wrong parameters given - provide an input file and an output folder" . "\n";
  exit;
}

my $total_stats = Statistics::Discrete->new();
my $ribs_stats = Statistics::Discrete->new();
my $updates_stats = Statistics::Discrete->new();



make_path($output_folder);


my $con_file_fh; # file handler
open($con_file_fh, ">",  $output_folder . "/consistency.txt") or die();
my $last_row = "";
my ($last_dump_time, $last_record_time,  $last_dump_type);
$last_dump_time = 0;
$last_record_time = 0;
$last_dump_type = "ribs";
my $i = 1;


my $agg_file_fh; # file handler
open($agg_file_fh, ">",  $output_folder . "/aggregated_rt.txt") or die();


my $delay_file_fh; # file handler
open($delay_file_fh, ">",  $output_folder . "/record_delay.csv") or die();
my $delay;


my $fh; # file handler
my $row;
open($fh, "<", $input_file) or die("Could not open file '$input_file' $!");

# input_file format:
# counter, record_time, dump_time, dump_type, dump_collector, rstatus, ts
my ($id, $record_time, $dump_time, $dump_type, $dump_collector, $rstatus, $ts);

my ($up_rt, $rib_rt);
$up_rt = 0;
$rib_rt = 0;
my ($last_up_rt, $last_rib_rt);
$last_up_rt = 0;
$last_rib_rt = 0;


while($row = <$fh>) {
  chomp($row);
  ($id, $record_time, $dump_time, $dump_type, $dump_collector, $rstatus, $ts) = split(/\s+/, $row);

  # PHASE 1: consistency check 

  if($record_time < $last_record_time) {
    print $con_file_fh "Error: " . $i . " recordtime not in order " . "\n";
    print $con_file_fh "\tBEFORE: " . $last_row . "\n";
    print $con_file_fh "\tAFTER: " . $row . "\n\n";
    $i++;
  }
  # if two entries have the same time then ribs have higher priority than updates
  if($record_time == $last_record_time and $last_dump_type eq "updates" and $dump_type eq "ribs") {
    print $con_file_fh "Error: " . $i . " ribs/updates not in order " . "\n";
    print $con_file_fh "\tBEFORE: " . $last_row . "\n";
    print $con_file_fh "\tAFTER: " . $row . "\n\n";
    $i++;
  }
  
  # PHASE 2: aggregate
  if($dump_type eq "ribs") {
    $rib_rt = $record_time;
    if($rib_rt != $last_rib_rt ) {
      print $agg_file_fh $id . "\t" . $rib_rt . "\t" . $dump_type .  "\t" . $ts ."\n";
    }
  }
  else{
    $up_rt = $record_time;
    if($up_rt != $last_up_rt ) {
      print $agg_file_fh $id . "\t" . $up_rt . "\t" . $dump_type . "\t" . $ts ."\n";
    }
  }

  # common statements
  $last_dump_time = $dump_time;
  $last_record_time = $record_time;
  $last_up_rt = $up_rt;
  $last_rib_rt = $rib_rt;
  $last_dump_type = $dump_type;     
  $last_row = $row;


  # PHASE 3: record delay
  $delay = $ts -$record_time;
  print $delay_file_fh $id . "," . $delay . "\n";
  $total_stats->add_data($delay);
  if($dump_type eq "ribs") {
    $ribs_stats->add_data($delay);
  }
  else{ 
    $updates_stats->add_data($delay);
  }

}

close($fh);
close($con_file_fh);
close($agg_file_fh);
close($delay_file_fh);


print_all_dist_on_file($total_stats, $output_folder . "/total_delay_dist.txt");
print_all_dist_on_file($ribs_stats, $output_folder . "/ribs_delay_dist.txt");
print_all_dist_on_file($updates_stats, $output_folder . "/updates_delay_dist.txt");



sub print_all_dist_on_file {
  my ($stat,$filename) = @_;
  my $fd = $stat->frequency_distribution();
  my $pmd = $stat->probability_mass_function();
  my $cdf = $stat->empirical_distribution_function();
  my $ccdf = $stat->complementary_cumulative_distribution_function();
  my $v;
  my @support = sort {$a<=>$b} keys %{$fd};
  my $file;
  open($file, ">", $filename) or die $!;
  print $file "#VAL\tFREQ\tPMD\tCDF\tCCDF\n";
  foreach $v (@support) {
    print $file $v . "\t";    
    print $file defined($fd->{$v}) ?  $fd->{$v}: 0;
    print $file "\t";
    print $file defined($pmd->{$v}) ? $pmd->{$v} : 0;
    print $file "\t";
    print $file defined($cdf->{$v}) ? $cdf->{$v} : 0;
    print $file "\t";
    print $file defined($ccdf->{$v}) ? $ccdf->{$v} : 0;
    print $file "\n";
  }
  close($file);
}
