Core Plugins{#plugins}
============

The Corsaro public release includes four Core Plugins which provide both useful
trace analysis functionality, and also serve as a template for developing new
plugins.

The four plugins are: 
 -# a \ref plugins_pcap filter
 -# our \ref plugins_ft packet aggregator
 -# an implementation of Moore's \ref plugins_dos detection 
	 algorithm \cite ton-moore-2006
 -# Smee - a port of the _iatmon_ tool \cite pam-brownlee-2012

Raw pcap{#plugins_pcap}
========

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

This plugin provides a good starting point for developing a _filter_ plugin,
which uses some criteria to only save a subset of packets to a pcap file. For
example, internally at CAIDA we have used a _SIP filter_ plugin to extract all
SIP packets from raw traces. While it would be possible to implement this using
the BPF filter feature of the \ref tool_corsaro tool, implementing a filter as a
plugin allows other plugins to continue to process the entire trace, thus
reducing re-work.

FlowTuple{#plugins_ft}
=========

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

In the current implementation, the tables are sorted before being written out
because we found with empirical testing that _gzip_ compression was maximized by
using the sort comparator found in \ref corsaro_flowtuple_lt.

See the \ref formats_ft section of the \ref formats page for information about
the format of the output created by the FlowTuple plugin.

We also provide an efficient tool for re-aggregating the FlowTuple data based on
a different time interval, a subset of key fields and/or a different value
field. See \ref tool_corsagg for a more detailed description of the tool.

RS DoS{#plugins_dos}
======

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

Smee{#plugins_smee}
====

Unfortunately Smee is still under development and will not be officially
included until the next release of Corsaro. Please contact
corsaro-info@caida.org if you wish to help alpha test it, or simply use the
[iatmon](http://www.caida.org/tools/measurement/iatmon/) tool.
