Core Plugins{#plugins}
============

[TOC]

The Corsaro 2.0.0 public release includes several Core Plugins which provide
both useful trace analysis functionality, and also serve as a template for
developing new plugins. These plugins can be loosely divided into three
categories: \ref plugins_aggregation, \ref plugins_metadata, and \ref
plugins_filters

All plugins must support processing packet trace files, but they may also
optionally support processing \ref plugins_ft files. Plugins that support
processing FlowTuple data are:
 - \ref plugins_ft
 - \ref plugins_anon
 - \ref plugins_pfx2as
 - \ref plugins_geodb
 - \ref plugins_filtergeo
 - \ref plugins_filterpfx

Aggregation/Analysis Plugins{#plugins_aggregation}
============================

FlowTuple{#plugins_ft}
---------

The FlowTuple plugin compresses a trace by counting the number of packets which
share a common set of header fields. This is conceptually similar to a NetFlow
report, but with a set of header fields specifically tailored to facilitate
darknet data analysis.

We selected the fields for inclusion in the tuple based reviewing the types of
analysis performed over the last decade using UCSD Network Telescope data. We
found a combination of eight fields which would allow the majority of analysis
to be carried out without needing to resort to full pcap traces.

The eight fields that the FlowTuple plugin aggregates packets based on are:

 -# Source IP address
 -# Destination IP Address
 -# Source Port 
 -# Destination Port
 -# Protocol
 -# TTL
 -# TCP Flags
 -# IP Length
 
Note, if the Protocol value is 1 (ICMP), then the Source and Destination Port
fields are used to represent the ICMP Type and Code fields, respectively.
 
The flows also have an extra implicit field, the _class_ that the packets belong
to. There are currently three possible classes:
 
 -# Backscatter
 -# ICMP Request
 -# Other
 
These classes have been derived from the CAIDA _crl_attack_flow_ software. The
logic to classify a packet into a class is contained in the \ref
flowtuple_classify_packet function in the \ref corsaro_flowtuple.c file.
 
The plugin maintains a table of flows for each class, and simply counts the
number of packets that belong to each flow in a given interval. At the end of an
interval, the plugin traverses each table and writes out a series of key-value
pairs, where the key is the 8 fields and the value is the number of packets in
the flow. The tables are then cleared and the process begins again for the next
interval.

By default, flow tables are sorted before being written out because we found
with empirical testing that _gzip_ compression was maximized by using the sort
comparator found in \ref corsaro_flowtuple_lt.

See the \ref formats_ft section of the \ref formats page for information about
the format of the output created by the FlowTuple plugin.

We also provide an efficient tool for re-aggregating the FlowTuple data based on
a different time interval, a subset of key fields and/or a different value
field. See \ref tool_corsagg for a more detailed description of the tool.

### Run-time Options ###

~~~
plugin usage: flowtuple [-s]
       -s            disable flowtuple output sorting
~~~

The FlowTuple plugin has one optional argument, `-s`, to disable output sorting.

RS DoS{#plugins_dos}
------

The RS DoS plugin uses heuristics described by Moore et al. in 
\cite ton-moore-2006 to detect backscatter packets caused by Randomly Spoofed
Denial of Service attacks, it then groups the backscatter by suspected attack
victim and reports statistics about each attack.

In addition to keeping high-level statistics about the attack, the plugin also
preserves a copy of the initial packet observed to be a part of the attack.

@todo describe the stats kept by this plugin and what they mean.

This plugin makes use of the plugin chaining feature described in \ref
arch_plugins to detect backscatter packets based on the classification performed
by the \ref plugins_ft plugin. As such, the RS DoS plugin requires the \ref
plugins_ft plugin to be enabled.

See \ref formats_dos section of the \ref formats page for information about the
format of the output files written by the RS DoS plugin.

### Run-time Options ###

The RS DoS plugin currently has no run-time configuration options. These may be
added in a subsequent release. Please contact corsaro-info@caida.org if this
would be of benefit to you.

Smee{#plugins_smee}
----

Smee uses the same analysis library as the _IATmon_ tool. See 
\cite pam-brownlee-2012 and the
[iatmon](http://www.caida.org/tools/measurement/iatmon/) project page for more
information.

### Run-time Options ###

~~~
plugin usage: smee [-d] [-i interval] [-l meter_loc] [-L max_src_life] [-s max_srcs] -a prefix
       -a            local prefix (-a can be specified multiple times)
       -i            interval between writing summary files (secs) (default: 3600)
       -l            meter location (default: XXX)
       -L            max lifetime for source to stay in hashtable (secs) (default: 3600)
       -m            memory size allocated for source hash table (in KB) (default: 4000000)
       -s            write the source tables to a file (disables summary tables)
~~~

### Installation Instructions ###

Smee requires the third-party _libsmee_ library which is currently only
available as a part of the Corsaro release. To use the Smee plugin, you will
first need to install the _libsmee_ library located in the _thirdparty_
directory of the Corsaro tarball (if you do not have root access see the note
below).

~~~{.sh}
cd thirdparty
tar zxf libsmee-2.2.2.tar.gz
cd libsmee-2.2.2
./configure
make
make install
~~~

At this point, you should be able to build Corsaro using the `--with-smee`
option to _configure_. 

#### Non-root Instructions ####

If you are building Corsaro on a machine that you do not have root access to (or
do not want to install _libsmee_ into a system-wide location), you can use the
`--prefix` option to _configure_ when building smee, as follows:

~~~{.sh}
cd thirdparty
tar zxf libsmee-2.2.2.tar.gz
cd libsmee-2.2.2
./configure --prefix=$HOME/libsmee
make
make install
~~~

And then when configuring Corsaro, do the following:

~~~{.sh}
./configure CPPFLAGS="-I$HOME/libsmee/include" LDFLAGS="-L$HOME/libsmee/lib" --with-smee"
make
~~~

Meta-data Plugins{#plugins_metadata}
=================

Raw pcap{#plugins_pcap}
--------

The simplest plugin which ships with Corsaro is the pcap pass-through filter. It
simply captures packets and writes them out to a file in pcap format. 

If Corsaro is operating on existing traces files, the output file will be
identical to the input (though potentially a different size due to
compression). In testing we find that even using threaded IO for the _gzip_
compression, the volume of data generated causes a bottle-neck in processing. As
such, we strongly recommend against enabling the pcap plugin when processing
existing trace files.

The pcap plugin becomes more useful when Corsaro is attached to a live
interface. It allows Corsaro to simultaneously capture raw trace data (for
archival and later analyis) and perform real-time analysis and aggregation with
other plugins. As noted earlier, the _gzip_ compression can cause a bottle-neck
in processing when the thread writing the pcap data cannot clear the write
buffer fast enough. This can be alleviated somewhat by tweaking the buffer size
in libtrace.

The pcap plugin can also be useful in conjunction with one of the \ref
plugins_filters for extracting a subset of packets from a trace file.

### Run-time Options ###

The pcap plugin currently has no run-time configuration options.

Cryto-PAn Anonymization{#plugins_anon}
-----------------------

The Crpyto-PAn anonymization plugin uses the Corsaro implementation of the
[Crypto-PAn](http://www.cc.gatech.edu/computing/Telecomm/projects/cryptopan/)
algorithm to anonymize source and/or destination IP addresses in packet headers.

This implementation is adapted from the
[traceanon](http://wand.net.nz/trac/libtrace/wiki/TraceAnon) tool distributed
with [libtrace](http://research.wand.net.nz/software/libtrace.php).

This plugin writes no output itself, but can be chained with other plugins (such
as \ref plugins_ft or \ref plugins_pcap) to produce anonymized output.

### Run-time Options ###

~~~
plugin usage: anon [-sd] [-t encryption_type] encryption_key[prefix]
       -d            enable destination address encryption
       -s            enable source address encryption
       -t            encryption type (default: cryptopan)
                     must be either 'cryptopan', or 'prefix'
~~~

The only mandatory argument is the encryption key (or prefix when using the
prefix substitution mode). The key can be up to 32 bytes and will be padded with
NULLs. If using prefix substitution, the prefix to substitute must be given.

There are three optional arguments: `-d` enables encryption of the destination
address, `-s` enables encryption of the source address (the default is no
encryption), and `-t` specifies the encrpytion type (the default is
`cryptopan`).

Prefix-to-AS{#plugins_pfx2as}
------------

The Prefix-to-AS ASN Lookup plugin uses
[CAIDA's Prefix-to-AS](http://www.caida.org/data/routing/routeviews-prefix2as.xml)
databases to determine the AS that the source IP of a packet belongs to. The
plugin does not write any data, instead it registers as a Geolocation Provider
(see \ref arch_geodb) to annotate packets as they are processed. Thus later
plugins in the chain can leverage the ASN for further analysis.

### Run-time Options ###

~~~
plugin usage: pfx2as [-c] -f pfx2as_file
       -c            cache the results for each IP
       -f            pfx2as file to use for lookups
~~~

The pfx2as plugin has one mandatory argument: `-f`, which specifies the pfx2as
database file to use for ASN lookups. Database files can be downloaded from the
[Prefix-to-AS](http://www.caida.org/data/routing/routeviews-prefix2as.xml) page
of the CAIDA website.

There is also one optional argument: `-c`, which causes the plugin to cache
IP-to-ASN results. This is useful if processing traces which have many repeated
IP addresses. Beware that the cache may grow large if running over long trace
files.

Geolocation{#plugins_geodb}
-----------

The geolocation plugin provides per-packet geographic annotation using either
[Maxmind](http://www.maxmind.com/en/home), or Digital Envoy's
[NetAcuity](http://info.digitalelement.com/us/) databases. Maxmind provides a
free version of their database,
[Maxmind Geo-Lite](http://dev.maxmind.com/geoip/legacy/geolite) which is in CSV
format and fully supported by this plugin.

The NetAcuity database support requires an ASCII-dump version of the NetAcuity
Edge database (not a free product) which has been pre-processed into a format
similar to the Maxmind CSV databases. For more information about pre-processing
the database, please contact corsaro-info@caida.org.

This plugin writes no output data, instead it registers as a Geolocation
Provider (see \ref arch_geodb) and annotates packets with geographic information
based on the source IP address. Thus later plugins (such as the \ref
plugins_filtergeo plugin), can leverage the meta-data for further analysis.

### Run-time Options ###

~~~
plugin usage: geodb [-p format] (-l locations -b blocks)|(-d directory)
       -d            directory containing blocks and location files
       -b            blocks file (must be used with -l)
       -l            locations file (must be used with -b)
       -p            database format (default: maxmind)
                       format must be one of:
                       - maxmind
                       - netacq-edge
~~~

The `geodb` plugin has one optional argument, `-p` which specifies the type of
geolocation database to use. The default database type is `maxmind`.

Additionally, both _blocks_ and _locations_ database files must be
specified. This can be done one of two ways:
 - explicitly using both the `-b` and the `-l` arguments, or
 - implicitly using the `-d` argument and providing a directory containing both
   files
 
If the `-d` option is used to specify a directory, the plugin will search given
directory for a _blocks_ file named `GeoLiteCity-Blocks.csv.gz`, and a locations
file named `GeoLiteCity-Location.csv.gz`. If your database files use other
names, then the `-b` and `-l` arguments must be used instead.

Filter Plugins{#plugins_filters}
==============

Filter plugins can be considered a special class of meta-data plugins. They
perform some analysis on each packet and determine whether subsequent plugins
_should_ ignore it. If a plugin is to be ignored (i.e. removed), a filter plugin
will set the \ref CORSARO_PACKET_STATE_IGNORE flag. Plugins should then consider
this flag when processing packets. 

For example, the \ref plugins_ft plugin ignores any packets for which the ignore
flag is set. This has the effect of removing these packets from the resulting
FlowTuple file. 

Note that setting the ignore flag does not guarantee that subsequent plugins
will ignore the packet. It is up to each plugin to determine whether they will
obey the ignore request.

Geographic Filter{#plugins_filtergeo}
-----------------

The geographic filter plugin allows a subset of packets to be extracted from the
input based on a given list of country codes. It can either extract only packets
that belong to the given countries, or only those packets which do not.

This plugin requires that a geolocation provider plugin which populates the
_country code_ field (currently only the \ref plugins_geodb plugin) is also
enabled, and run _prior_ to this. This plugin _does not_ do any geolocation
itself.

### Run-time Options ###

~~~
plugin usage: filtergeo [-di] [-c country [-p country]] [-f country_file]
       -c            country code to match against, -c can be used up to 100 times
                     Note: use 2 character ISO 3166-1 alpha-2 codes
       -f            read countries from the given file
       -i            invert the matching (default: find matches)
~~~

The `filtergeo` plugin operates based on a list of 2 character
[ISO 3166-1 alpha-2 country codes](http://en.wikipedia.org/wiki/ISO_3166-1_alpha-2). These
can be passed to the plugin in one of two ways:
 - on the command line using the `-c` argument (can be used multiple times), or
 - in a file which is passed using the `-f` option
 
The `-c` option is best suited for use with a small number of countries. For a
large set, please use the file option.

The plugin also accepts one optional argument, `-i`, which inverts the
matching. The default behavior is to _remove_ all packets which _do not_ match
one of the input countries. Using the `-i` option inverts this so that packets
which _do_ match are _removed_.

Prefix Filter{#plugins_filterpfx}
-------------

The prefix filter plugin operates in much the same manner as the
\ref plugins_filtergeo plugin, except it takes as input, a list of IPv4 prefixes. 

The plugin uses longest-prefix matching to determine whether the source (or
destination of given the `-d` option) IP address of a packet is within one of
the specified prefixes.

### Run-time Options ###

~~~
plugin usage: filterpfx [-di] [-p pfx [-p pfx]] [-f pfx_file]
       -d            use destination address (default: source)
       -f            read prefixes from the given file
       -i            invert the matching (default: find matches)
       -p            prefix to match against, -p can be used up to 100 times
~~~

The `filterpfx` plugin operates based on a list of IPv4 prefixes in
[CIDR notation](http://en.wikipedia.org/wiki/CIDR_notation#CIDR_notation). These
prefixes can be passed to the plugin in one of two ways:
 - on the command line using the `-p` argument (can be used multiple times), or
 - in a file which is passed using the `-f` option
 
The `-p` option is best suited for use with a small number of prefixes. For a
large set, please use the file option.

The plugin also accepts one optional argument, `-i`, which inverts the
matching. The default behavior is to _remove_ all packets which _do not_ match
one of the input prefixes. Using the `-i` option inverts this so that packets
which _do_ match are _removed_.
