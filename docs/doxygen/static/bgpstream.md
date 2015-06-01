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


Download            {#download}
========
bgpstream 1.0.0 can be downloaded [here](http://www.caida.org/~chiara/bgpstream-doc/bgpstream-1.0.0.tar.gz).



Quick Start               {#qstart}
===========

<!---
If you want to just dive right in and get started using BGP Stream, take a look at
the \ref quickstart "Quick Start" guide.
-->

In order to install bgpstream  you need to follow the list of instructions below:


    $ ./configure
    $ make
    $ make check
    $ (sudo) make install


Use the --prefix option to install bgpstream and bgpreader in a local directory.

The procedure above builds and installs:
- the *bgpstream* library
- the *bgpreader* tool

If you are interested in installing the python bindings, then continue
the installation following the instructions [here](http://www.caida.org/~chiara/bgpstream-doc/pybgpstream/install.html).


Dependencies        {#dependencies}
============

BGP Stream is written in C and should compile with any ANSI compliant C Compiler
which supports the C99 standard. Please email bgpstream-info@caida.org with any
issues.

Building BGP Stream requires:
- [libtrace](http://research.wand.net.nz/software/libtrace.php)
version 3.0.14 or higher
    - a working version of libtrace is available at http://research.wand.net.nz/software/libtrace/libtrace-3.0.22.tar.bz2 

- [sqlite3](https://www.sqlite.org/) c library
     - a working version of sqlite3 is available at https://www.sqlite.org/2015/sqlite-autoconf-3081002.tar.gz

- [mysql](https://www.mysql.com/) c library, version 5.5  or higher
  - a working version of mysql is available at http://downloads.mysql.com/archives/get/file/mysql-connector-c-6.1.5-src.tar.gz

If the above libraries are not in the system library paths, then you
can specify their paths at configuration time:

    $ ./configure CPPFLAGS='-I/path_to_libs/include' LDFLAGS='-L/path_to_libs/lib'
    


bgpreader tool usage               {#usage}
===============
bgp reader is a command line tool that prints to standard output
information about a bgp stream.

    usage: bgpreader -d <interface> [<options>]

        -d <interface>    use the given data interface to find available data
                                    available data interfaces are:
            singlefile     Read a single mrt data file (a RIB and/or an update)
            csvfile        Retrieve metadata information from a csv file
            sqlite         Retrieve metadata information from a sqlite database
            mysql          Retrieve metadata information from the bgparchive mysql database

        -o <option-name,option-value>*    set an option for the current data interface.
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


Example:

print out all the bgp elems observed in the
*routeviews.route-views.jinx.updates.1427846400.bz2* file from
00:00:15 to 00:01:30


    $ bgpreader -dsinglefile -oupd-file,./test/routeviews.route-views.jinx.updates.1427846400.bz2 -W1427846415,1427846490 -e

    1427846430|196.223.14.55|30844|W|185.75.149.0/24|||||
    1427846430|196.223.14.55|30844|W|103.47.62.0/23|||||
    1427846430|196.223.14.55|30844|W|132.8.48.0/20|||||
    1427846430|196.223.14.55|30844|W|132.8.64.0/18|||||
    1427846430|196.223.14.55|30844|W|132.22.0.0/16|||||
    1427846430|196.223.14.55|30844|W|131.34.0.0/16|||||
    1427846430|196.223.14.55|30844|W|129.54.0.0/16|||||
    1427846430|196.223.14.55|30844|A|41.159.135.0/24|196.223.14.55|30844 6939 12956 6713 16058|16058||
    1427846430|196.223.14.55|30844|A|84.205.73.0/24|196.223.14.55|30844 6939 12654|12654||



bgpstream C library tutorial               {#tutorial}
=====================

Below is a simple example that shows how to use most of the C library
API functions. The example is fully functioning and it can be run within the *test*
folder included in the distribution.

    $ cd bgpstream-1.0.0/test
    $ gcc ./tutorial.c  -lbgpstream -o ./tutorial
    $ ./tutorial
     Read 1544 elems


The program reads information from the example sqlite
database provided with the distribution and it counts the elems that match
the filters (collectors, record type, and time).


~~~~~~~~~~~~~~~~~~~~~{.c}
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>

#include "bgpstream.h"

int
main()
{
  /* Allocate memory for a bgpstream instance */
  bgpstream_t *bs = bs = bgpstream_create();
  if(!bs) {
    fprintf(stderr, "ERROR: Could not create BGPStream instance\n");
    return -1;
  }

  /* Allocate memory for a re-usable bgprecord instance */
  bgpstream_record_t *bs_record = bgpstream_record_create();
  if(bs_record == NULL)
    {
      fprintf(stderr, "ERROR: Could not create BGPStream record\n");
      return -1;
    }

  
  /* Configure the sqlite interface */
  bgpstream_data_interface_id_t datasource_id = bgpstream_get_data_interface_id_by_name(bs, "sqlite");
  bgpstream_set_data_interface(bs, datasource_id);

  /* Configure the sqlite interface options */
  bgpstream_data_interface_option_t *option = 
    bgpstream_get_data_interface_option_by_name(bs, datasource_id,
                                                "db-file");
  bgpstream_set_data_interface_option(bs, option, "./sqlite_test.db");

  /* Select bgp data from RRC06 and route-views.jinx collectors only */
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, "rrc06");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, "route-views.jinx");

  /* Process updates only */
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_RECORD_TYPE, "updates");

  /* Select a time interval to process:
   * Wed, 01 Apr 2015 00:02:30 GMT -> Wed, 01 Apr 2015 00:05:00 UTC */
  bgpstream_add_interval_filter(bs,1427846550,1427846700);

  /* Start bgpstream */
  if(bgpstream_start(bs) < 0) {
    fprintf(stderr, "ERROR: Could not init BGPStream\n");
    return -1;
  }

  int get_next_ret = 0;
  int elem_counter = 0;

  /* pointer to a bgpstream elem, memory is borrowed from bgpstream,
   * use the elem_copy function to own the memory */
  bgpstream_elem_t *bs_elem = NULL;

  /* Read the stream of records */
  do
    {
      /* get next record */
      get_next_ret = bgpstream_get_next_record(bs, bs_record);
      if(get_next_ret && bs_record->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD)
        {
          while((bs_elem = bgpstream_record_get_next_elem (bs_record)) != NULL)
            {
              elem_counter++;
            }
        }
    }
  while(get_next_ret > 0);

  printf("\tRead %d elems\n", elem_counter);

  /* de-allocate memory for the bgpstream */
  bgpstream_destroy(bs);

  /* de-allocate memory for the bgpstream record */
  bgpstream_record_destroy(bs_record);

  
  return 0;
}

~~~~~~~~~~~~~~~~~~~~~


Documentation       {#docs}
=============
This online [BGP Stream API] is currently the best source of information
about using BGP Stream, please take a look at the installed headers:

- **bgpstream.h**
- **bgpstream_record.h**
- **bgpstream_elem.h**

<!---
This online [BGP Stream Documentation](TODO) is the best source of information
about using BGP Stream. It contains full API documentation, usage instructions
for the BGP Stream tools. It also has tutorials about using the libbgpstream
library to perform analysis of BGP data.
-->
