Overview {#index}
========

@note Both BGP Stream and this documentation are still under active development and
features will likely change between versions.
@note Please contact bgpstream-info@caida.org with any questions and/or suggestions.

<!---
Introduction        {#intro}
============
@todo write intro
Download            {#download}
========
The latest version of BGP Stream is
[$(BGPSTREAM_VERSION)](http://www.caida.org/tools/XXX/bgpstream-1.0.0.tar.gz),
released on @todo add release date and update download link.
We also maintain a detailed \ref changelog.
-->

Quick Start               {#qstart}
===========

<!---
If you want to just dive right in and get started using BGP Stream, take a look at
the \ref quickstart "Quick Start" guide.
-->
In order to install bgpstream  you need to follow the list of instructions below:
```sh
$ sh ./autogen.sh  
$ ./configure
$ make
$ (sudo) make install
```
The procedure above builds and installs:
- the *bgpstream* library
- the *bgpreader* tool



Dependencies        {#dependencies}
============

BGP Stream is written in C and should compile with any ANSI compliant C Compiler
which supports the C99 standard. Please email bgpstream-info@caida.org with any
issues.

Building BGP Stream requires:
- [libtrace](http://research.wand.net.nz/software/libtrace.php)
version 3.0.14 or higher
- [sqlite3](https://www.sqlite.org/) c library
- [mysql](https://www.mysql.com/) c library

If the above libraries are not in the system library paths, then you
can specify their paths at configuration time:

```sh
$ sh ./autogen.sh  
$ ./configure CPPFLAGS='-I/path_to_libs/include' LDFLAGS='LI/path_to_libs/lib'
$ make
$ (sudo) make install
```


bgpreader usage               {#usage}
===============

@todo add usage

```sh
usage: bgpreader -d <interface> [<options>]
   -d <interface> use the given data interface to find available data
     available data interfaces are:
       singlefile     Read a single mrt data file (a RIB and/or an update)
       csvfile        Retrieve metadata information from a csv file
       sqlite         Retrieve metadata information from a sqlite database
       mysql          Retrieve metadata information from the bgparchive mysql database
   -o <option-name,option-value>*
                  set an option for the current data interface.
                    use '-o ?' to get a list of available
                    options for the current data interface.
                    (data interface must be selected using -d)
   -P <project>   process records from only the given project (routeviews, ris)*
   -C <collector> process records from only the given collector*
   -T <type>      process records with only the given type (ribs, updates)*
   -W <start,end> process records only within the given time window*
   -R <period>    process a rib files every <period> seconds (bgp time)
   -b             make blocking requests for BGP records
                  allows bgpstream to be used to process data in real-time
   -r             print info for each BGP record (in bgpstream format) [default]
   -m             print info for each BGP valid record (in bgpdump -m format)
   -e             print info for each element of a valid BGP record
   -h             print this help menu
   * denotes an option that can be given multiple times

Data interface options for 'singlefile':
   rib-file       rib mrt file to read (default: not-set)
   upd-file       updates mrt file to read (default: not-set)

Data interface options for 'csvfile':
   csv-file       csv file listing the mrt data to read (default: not-set)

Data interface options for 'sqlite':
   db-file        sqlite database (default: not-set)

Data interface options for 'mysql':
   db-name        name of the mysql database to use (default: bgparchive)
   db-user        mysql username to use (default: bgpstream)
   db-password    mysql password to use (default: not-set)
   db-host        hostname/IP of the mysql server (default: not-set)
   db-port        port of the mysql server (default: not-set)
   db-socket      Unix socket of the mysql server (default: not-set)
   ris-path       Prefix path of RIS data (default: not-set)
   rv-path        prefix path of RouteViews data (default: not-set)

```



Documentation       {#docs}
=============

This online [BGP Stream Documentation](TODO) is the best source of information
about using BGP Stream. It contains full API documentation, usage instructions
for the BGP Stream tools. It also has tutorials about using the libbgpstream
library to perform analysis of BGP data.
