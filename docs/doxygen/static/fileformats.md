File Formats {#formats}
============

[TOC]

Because corsaro is designed to be modular, there is an overall global output
format and then there are per-plugin output formats. Plugins should attempt
to follow the same conventions as Corsaro, but this is not compulsory or enforced.

Corsaro has an optional commandline paramter which allows the output 'mode' to
be set to either 'ascii' for (somewhat) human-readable output or 'binary' for a
compact binary representation of the data. The binary representation tends to
perform slightly better under compression than the ascii does, thus resulting in
smaller (compressed) output files.

This page should not be considered a definitive guide to output formats other
than that of the global output file. The formats for the two core plugins (\ref
plugins_ft and \ref plugins_dos) are included for reference. For documentation
about other CAIDA-produced plugins please refer to the internal
[Corsaro wiki](https://trac.figaro.caida.org/corsaro/), or email
corsaro-info@caida.org.

General Conventions {#formats_conventions}
===================

Plugins may output data in whatever format they like, but there are several
conventions that Corsaro uses in the output files that while not enforced in any
way, should be followed by plugins for compatibility:

 - Binary data MUST be byte-aligned.
 - Values SHOULD be written to binary files in network byte order.
 - Non-data attributes SHOULD be prefixed with `#` in ASCII output to allow 
   easy filtering (e.g. `fgrep -v #`)
 - Time values SHOULD be a `uint32_t` number giving seconds since the epoch 
   (unix time)
  - Times that refer to Corsaro operations SHOULD be from the monitor running 
    Corsaro
  - Times that refer to packets SHOULD be from the trace

In addition to these data conventions, we also use the following conventions in this manual to describe the format of the data:

 - Placeholder values are given inside angle brackets
  - e.g. `<trace_uri>` would be replaced with a trace uri string in the actual 
    output
 - Bit lengths for binary fields are given using array notation
  - e.g. `<version_major[8]>` indicates an 8 bit value describing the major 
    version number.

Global Output File {#formats_global}
==================

The Corsaro global output file follows the following structure:

ASCII
-----

~~~
# CORSARO_VERSION <corsaro_version>
# CORSARO_INITTIME <local_unix_time>
# CORSARO_INTERVAL <interval_seconds>
# CORSARO_TRACEURI <trace_uri> **optional**
# CORSARO_PLUGIN <plugin1_name>
# CORSARO_PLUGIN <plugin2_name>
...
# CORSARO_INTERVAL_START <interval_number> <trace_time>
# CORSARO_PLUGIN_DATA_START <plugin1>
<plugin1_output>
# CORSARO_PLUGIN_DATA_END <plugin1>
# CORSARO_PLUGIN_DATA_START <plugin2>
<plugin2_output>
# CORSARO_PLUGIN_DATA_END <plugin2>
...
# CORSARO_INTERVAL_END <interval_number> <trace_time>
...
# CORSARO_PACKETCNT <packet_count>
# CORSARO_FIRSTPKT <trace_time>
# CORSARO_LASTPKT <trace_time>
# CORSARO_FINALTIME <local_unix_time>
# CORSARO_RUNTIME <seconds>
~~~

Binary
------

### Header Format ###
~~~
<magic_number[32]><header_magic[32]>
<version_major[8]><version_minor[8]>
<local_init_time[32]>
<interval_len[16]>
<trace_uri_len[16]><trace_uri[trace_uri_len*8]>
<plugin_cnt[16]><plugin_list[plugin_cnt*16]>
~~~

### Interval Format ###

\verbatim
| <magic_number[32]><interval_magic[32]>
| <interval_number[16]><interval_start_time[32]>
|  | <magic_number[32]><data_magic[32]><plugin_id[16]>
|  |   -- plugin data (variable length)
|  | <magic_number[32]><data_magic[32]><plugin_id[16]>
|  -- repeats, one per plugin listed in header
| <magic_number[32]><interval_magic[32]>
| <interval_number[16]><interval_end_time[32]>
-- repeats, one per interval in trace
\endverbatim

### Trailer Format ###
~~~
<magic_number[32]><trailer_magic[32]>
<packetcnt[64]>
<firstpkt_time[32]>
<lastpkt_time[32]>
<local_final_time[32]>
<runtime[32]>
EOF
~~~

Magic Numbers:
Field | ASCII | Hex
------|-------|-----
Magic Number | `EDGR` | `0x45444752`
Header Magic | `HEAD` | `0x48454148`
Interval Magic | `INTR` | `0x494E5452`
Data Magic | `DATA` | `0x44415441`
Trailer Magic | `FOOT` | `0x464F4F54`

The plugin list in the Binary output format is an array of `uint16_t` numbers
which correspond to the plugin IDs given by the \ref corsaro_plugin_id enum.

Note, the pluin ids 0x4544, 0x4845 and 0x464F are reserved for corsaro use

FlowTuple {#formats_ft}
=========

Global Output
-------------

### ASCII ###
NONE

### Binary ###
NONE

Plugin Output
-------------
### ASCII ###
Note, all values are in decimal except the ip addresses, and the TCP flags.
IP addresses are given in dotted decimal, and TCP flags are in hex.
~~~
START <class_name> <key_cnt>
<src_ip>|<dst_ip>|<src_port>|<dst_port>|<protocol>|  \
	<tcp_flags>|<ttl>|<ip_len>,<value>
END <class_name>
~~~

### Binary ###
~~~
<magic_number[32]><class_id[16]><key_cnt[32]>
<src_ip[32]><dst_ip[24|32]><src_port[16]><dst_port[16]>	\
	<protocol[8]><tcp_flags[8]><ttl[8]><ip_len[16]> \
	<value[32]>		# 160 bit record length
<magic_number[32]><class_id[16]>
~~~

Two bit lengths are given for the `dst_ip` field. If Corsaro is built using the
`--with-slash-eight=X` option only the three least significant bytes (24 bits)
of destination IP addresses are serialized (the assumption being that the most
significant byte can easily be re-created upon deserialization). The 32 bit
length is for the normal behavior where all 4 bytes of the address is stored.

Magic Number is `SIXT` (`0x53495854`) when using /8 optimaizations, `SIXU`
(`0x53495855`) without.

For a list of class IDs, see \ref corsaro_flowtuple_class_type.

RS DoS {#formats_dos}
======

Global Output
-------------

### ASCII ###
~~~
mismatch: <number_mismatched_ips>
attack_vectors: <vector_cnt>
non-attack_vectors: <nonattack_cnt>
~~~
### Binary ###
~~~
<number_mismatched_ips><vector_cnt><nonattack_cnt>
~~~

Plugin Output
-------------

### ASCII ###
Note, all values are in decimal form except the target IP address, which is
given in dotted decimal format.
~~~
<vector_cnt>
<target_ip>,<#attacker_ips>,<#interval_attacker_ips><#attacker_ports>,
	<#target_ports>,<packet_cnt>,<interval_packet_cnt>,
	<byte_cnt>,<interval_byte_cnt>,<max_ppm>,
	<start_time_sec>.<start_time_usec>,
	<latest_time_sec>.<latest_time_usec>
START PACKET
<libtrace packet dump>
END PACKET
~~~

If Corsaro is built with a version of _libtrace_ that does not include
_libpacketdump_ then the initial packet will not be output when using the \ref
tool_cors2ascii tool.

### Binary ###
~~~
<vector_cnt[32]>
<target_ip[32]><#attacker_ips[32]><#interval_attacker_ips[32]>
	<#attacker_ports[32]><#target_ports[32]>
	<packet_cnt[64]><interval_packet_cnt[32]>
	<byte_cnt[64]><interval_byte_cnt[32]>
	<max_ppm[64]>
	<start_time_sec[32]><start_time_usec[32]>
	<latest_time_sec[32]><latest_time_usec[32]>
	<pkt_length[32]><initial_packet[pkt_length]>
~~~

There is a known bug in the binary output of the RS DoS plugin which causes it
not to write a magic number to the output file. This has the unfortunate effect
of requiring RS DoS files to be correctly named so that plugin detection does
not need to inspect the file contents as described in the \ref arch_corsaro_in
section.
