Tools               {#tools}
=====

[TOC]

Corsaro ships with several tools which leverage the _libcorsaro_ library. This
section describes the purpose of each tool and how to use it.

corsaro     {#tool_corsaro}
=======

This is the main tool in the Corsaro suite, it provides a lightweight wrapper
around the \ref arch_corsaro_out features of _libcorsaro_.  The _corsaro_ tool
processes trace files and uses a set of plugins to analyze and generate
aggregated statistics about the packets they contain. For more information about
this process, see the \ref arch_corsaro_out and \ref arch_plugins sections of
this manual.

In addition to processing existing trace files, _corsaro_ can capture packets
from a live interface by using the special `pcapint:<interface>` trace URI
parameter. This feature is still in the alpha testing phase and so only has
limited functionality, e.g., output files are not rotated.

Usage
-----

~~~
usage: corsaro [-alP] -o outfile [-i interval] [-m mode] [-n name]
               [-p plugin] [-f filter] [-r intervals] trace_uri [trace_uri...]
       -a            align the end time of the first interval
       -o <outfile>  use <outfile> as a template for file names.
                      - %P => plugin name
                      - %N => monitor name
                      - see man strftime(3) for more options
       -f <filter>   BPF filter to apply to packets
       -i <interval> distribution interval in seconds (default: 60)
       -l            the input file has legacy intervals (FlowTuple only)
       -m <mode>     output in 'ascii' or 'binary'. (default: binary)
       -n <name>     monitor name (default: gibi.caida.org)
       -p <plugin>   enable the given plugin, -p can be used multiple times (default: all)
                     available plugins:
                      - anon
                      - flowtuple
                     use -p "<plugin_name> -?" to see plugin options
       -P            enable promiscuous mode on the input (if supported)
       -r            rotate output files after n intervals
       -R            rotate corsaro meta files after n intervals
~~~

### Required Arguments ###

_corsaro_ takes two mandatory arguments: an output filename template, and an
input trace URI. 

#### Output Filename Template ####

The output template must contain the string `%P` which will be expanded to the
name of the plugin (and possibly a plugin-specific identifier) for each file
created. _corsaro_ will scan the filename to determine which, if any,
compression should be used when creating the file. A `gz` extension will cause
`gzip` compression to be used, whereas a `bz2` compression will use `bzip`
compression. Uncompressed files will be created for all other extensions.

The template may optionally contain the string `%N` which will be expanded to
the monitor name. The monitor name can either be specified using the `-n` option
at run-time, or using the `--with-monitorname` option to `configure`.

In addition, the template can contain any of the specifiers supported by
`strftime(3)` which will be replaced with the appropriate representation of
start time of the first interval in the file.

For example, at CAIDA, we use: 
~~~
/path/to/corsaro/data/%N.%s.%P.cors.gz 
~~~

#### Input URI #### 

The input URI will most commonly just be the path to a file of any format
supported by `libtrace`. It can however also be a \ref plugins_ft file - this is
useful for re-processing data using additional plugins (many of the \ref plugins
support processing FlowTuple records). 

The IO library (_libwandio_) used by _libcorsaro_ can automatically detect
_gzip_ and _bzip_ compression if it is used. Multiple trace URIs can be supplied
and will be processed in the order they are listed. Take care to ensure packets
are sorted in chronological order - plugin behavior is undefined for unordered
packets.

### Optional Arguments ###

The remaining arguments are all optional and alter how the trace is
processed. 

 - `-a` align
  - aligns the end of the first interval (and therefore subsequent interval
    start/ends) to a time that is a multiple of the interval time. For example,
    if the interval length is 3600 (1hr), then the first interval will end at
    the top of the hour, thus aligning all further intervals to the hour.	
 - `-f` filter
  - specify a BPF string to use to filter packets
 - `-i` interval
  - determines the length of time (in seconds) included in each aggregation 
    interval
  - defaults to 60 (1 minute) 
  - see the \ref arch_intervals section of the \ref arch page 
 - `-l` legacy
  - indicates that the input \ref plugins_ft file has legacy-format intervals
 - `-m` mode
  - selects between output formats
  - `binary` and `ascii` are supported values
  - defaults to `binary`
 - `-n` name
  - the monitor name to use (overrides the name given by `--with-monitorname`)
  - will be inserted into output templates where the `%N` specifier is used
 - `-p` plugin
  - specify a plugin to use for analysis
  - can be used multiple times to specify a list of plugins
 - `-P` promiscuous
  - enables promiscuous mode on the capture interface
  - used when capturing from a live interface
 - `-r` rotate
  - specify the number of intervals after which output files should be rotated
  - defaults to no rotation
 - `-R` meta-rotate
  - specify the number of intervals after which meta-output (global and log)
    files should be rotated
  - defaults to the same value as `-r`

The output files generated by the _corsaro_ tool can be viewed either with a
standard text viewer (for the ASCII output format), or using the \ref
tool_cors2ascii tool (for the binary output format).

cors2ascii {#tool_cors2ascii}
==========

The _cors2ascii_ tool converts binary output from any corsaro plugin that
implements the \ref arch_corsaro_in API to an ASCII format. The output from
_cors2ascii_ depends on the specific plugin used, but the output will be in a
format which is mostly human-readable, as well as supporting ad-hoc analysis
scripts (e.g. written in Perl).

Currently _cors2ascii_ supports the \ref formats_ft and \ref formats_dos binary
output formats, as well as the Corsaro \ref formats_global. See the \ref formats
page for details about the output formats for plugins.

Usage
-----

    usage: cors2ascii input_file
	
_cors2ascii_ takes a single argument: the path to the file to be converted to
ASCII. Because _cors2ascii_ uses the \ref arch_io, _gzip_ and _bzip_ compressed
files are supported also.

cors-ft-aggregate {#tool_corsagg}
=================

The _cors-ft-aggregate_ tool re-aggregates \ref plugins_ft based on time and
sub-tuples.

The re-aggregation features of `cors-ft-aggregate` provide a powerful method for
analyzing specific dimensions of a dataset, much more efficiently and reliably
than parsing and manually aggregating the data output by `cors2ascii`.

The current version of the tool only supports the \ref formats_ft ASCII output
format. The fields of the tuple which are **not** included in the re-aggregation
will be zeroed out as shown in the example output below. Also, the tool does not
preserve the classes from the original data - tuples from all classes are
aggregated into a single table. Support for binary output and class preservation
is planned for a future release.

Usage
-----

~~~
usage: cors-ft-aggregate [-l] [-i interval] [-v value_field] -f field [-f field]... file_list
       -l            treat the input files as containing legacy format data
       -i <interval> new distribution interval in seconds. (default: 0)
                      a value of -1 aggregates to a single interval
                      a value of 0 uses the original interval
       -v <value>    field to use as aggregation value (default: packet_cnt)
       -f <field>    a tuple field to re-aggregate with

Supported field names are:
 src_ip, dst_ip, src_port, dst_port, protocol, ttl, tcp_flags, 
 ip_len, packet_cnt
~~~

### Required Arguments ###

  - `field`
   - include the specified field of the original tuple in the re-aggregated
     output
   - can be used multiple times to specify a list of fields
  - `file_list`
   - file containing an ordered list of \ref plugins_ft binary files to be
     re-aggregated
   - use `-` to read the list from stdin (for use with `find (1)` etc).
   
### Optional Arguments ###

 - `l[egacy]`
  - indicates that the input files use a legacy Corsaro format
  - only applicable for CAIDA data that was generated with a pre-alpha version
    of Corsaro
 - `interval`
  - new time interval to aggregate data to
  - must be longer than original interval
  - special value of `-1` indicates that all data should be aggregated into a
    single interval
  - `0` uses the original interval in the file (60 seconds for
    CAIDA data).
 - `value`
  - field which should be used as the value for each aggregated tuple
  - corresponds to `packet_cnt` in a raw \ref plugins_ft file
  - any field can be used
  - for all fields other than `packet_cnt`, the value will be the number of
    unique elements in the set
  - e.g. `src_ip` will give a value for each tuple which is the number of unique
    source IP addresses which match the sub-tuple (as specified by the `field`
    arguments)

### Example Uses ###

#### Per-Day Protocol Statistics ####

Re-aggregating data with a 24 hour interval, using `protocol` as the field, and
`src_ip` as the value, gives per-day tables describing the number of unique
source IP addresses observed for each protocol.

Command used:
~~~
find /path/to/flowtuple/data/ -type f -name "*.flowtuple.cors.gz" | sort | \
  cors-ft-aggregate -i 86400 -v src_ip -f protocol -
~~~

Sample output:
~~~
# CORSARO_INTERVAL_START 0 1325390400
0.0.0.0|0.0.0.0|0|0|0|0|0x00|0,551
0.0.0.0|0.0.0.0|0|0|1|0|0x00|0,5741
0.0.0.0|0.0.0.0|0|0|6|0|0x00|0,151336
0.0.0.0|0.0.0.0|0|0|8|0|0x00|0,2
0.0.0.0|0.0.0.0|0|0|17|0|0x00|0,1042968
0.0.0.0|0.0.0.0|0|0|28|0|0x00|0,5
~~~
Note, this output has been sorted in post-processing

The output shows the familiar \ref formats_ft ASCII format, albeit with all
fields except the protocol zeroed out. Also, the _packet count_ value has been
replaced with a count of the number of unique source IPs observed for the
corresponding sub-tuple over the interval. In this example, we can see that UDP
(protocol 17, 6th line of output) packets were received from a total of
1,042,968 different sources during the interval.

#### Per-Minute UDP SIP Packet Counts  ####

Re-aggregate and filter data using `protocol` and `dst_port` fields, leaving the
_interval_ and _value_ unchanged.

Command Used:
~~~
find /path/to/flowtuple/data/ -type f -name "*.flowtuple.cors.gz" | sort | \
  cors-ft-aggregate -i 0 -v packet_cnt -f protocol -f dst_port - | \
  fgrep -e "0.0.0.0|0.0.0.0|0|5060|17|" -e "CORSARO_INTERVAL_START"
~~~

Sample Output:
~~~
# CORSARO_INTERVAL_START 0 1325390400
0.0.0.0|0.0.0.0|0|5060|17|0|0x00|0,3577
# CORSARO_INTERVAL_START 1 1325390460
0.0.0.0|0.0.0.0|0|5060|17|0|0x00|0,3412
# CORSARO_INTERVAL_START 2 1325390520
0.0.0.0|0.0.0.0|0|5060|17|0|0x00|0,3018
~~~

We re-aggregate the data using the `protocol` and `dst_port` fields, maintaining
the original interval (60 seconds) and leaving the packet count as the value
field. We then use a simple `grep` to filter the output to only records which
have a destination port of `5060` (SIP) and a protocol value of `17` (UDP). In
this example, the first minute of data, which begins at 01/01/12 04:00 UTC
(1325390400) contained 3,577 packets to UDP port 5060.
Note, future versions of `cors-ft-aggregate` will directly support filters such
as this to greatly improve processing speed.

@todo extend to allow to write out to binary again 
@todo respect the tuple classes for reaggregation (currently classes are 
  discarded).
@todo add a BPF-like filter

cors-ft-timeseries.pl {#tool_timeseries}
==================

A simple script that reads ASCII flowtuple data from STDIN and sums the tuple
values in each interval. The output is of the format:

~~~
<interval_start_time>,<value_sum>
~~~

This is useful for converting the output from \ref tool_corsagg into a simple
time series. 

Usage
-----

~~~
usage: | cors-ft-timeseries.pl
~~~		

Because this script reads from STDIN, it should be piped the data to be
converted. If you have an existing ASCII-format FlowTuple file, simple do
something like:

~~~
cat <flowtuple_file> | cors-ft-timeseries.pl 
~~~

But more likely you will chain it directly to \ref tool_corsagg like so:

~~~
cors-ft-aggregate [options] <input_file> | cors-ft-timeseries.pl
~~~

cors-splitascii.pl {#tool_splitascii}
==================

Takes the output from \ref tool_cors2ascii and splits each interval into a
separate file. Useful for generating a single file per interval for further
processing without the need to detect interval start and end records.

Usage
-----

~~~
usage: cors-splitascii.pl output_pattern [input_file]
 where output_pattern must include %INTERVAL% to be replaced with
 the interval timestamp
~~~		

Similar to the \ref tool_corsaro tool, an output file name template must be
specified, in which the string `%INTERVAL%` will be replaced with the interval
start time in each output file.

The input file can be any file which contains ASCII formatted corsaro interval
data. To read data from stdin, use `-` as the input file.
This allows `cors-splitascii.pl` to be chained directly to `cors2ascii` like
this:
~~~
cors2ascii <input_file> | cors-splitascii.pl <output_pattern> -
~~~


cors-trace2tuple {#tool_trace2tuple}
================

Quickly converts a trace file into an easily parseable list of tuples.

While `cors-trace2tuple` does not use the _libcorsaro_ framework, it is useful
for quickly generating a high-level representation of the packets contained in a
trace file.

Usage
-----

    \verbatim Usage: cors-trace2tuple [-H|--libtrace-help] [--filter|-f bpf ]... libtraceuri... \endverbatim

Each packet in the input trace (which is accepted by the optional BPF filter) is
represented by a single line in the tab-separated ASCII output.

Output is in the following format:
~~~
<timestamp> <src_ip> <dst_ip> <src_port> <dst_port> <protocol> <ipid> <ip_len>
~~~

cors-plugin-bootstrap {#tool_corsplugstrap}
=====================

<i>Not yet implemented</i>
