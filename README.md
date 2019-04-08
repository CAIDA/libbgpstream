BGPStream
=========

BGPStream is an open-source software framework for the analysis of both
historical and real-time Border Gateway Protocol (BGP) measurement data.

For a detailed description of BGPStream as well as documentation and tutorials,
please visit http://bgpstream.caida.org.

Quick Start
-----------

To get started using BGPStream, either download the latest
[release tarball](http://bgpstream.caida.org/download), or clone the
[GitHub repository](https://github.com/CAIDA/bgpstream).

You will also need the _libcurl_ and
[wandio](http://research.wand.net.nz/software/libwandio.php) libraries installed
before building BGPStream (libcurl **must** be installed prior to building
wandio).

In most cases, the following will be enough to build and install BGPStream:
~~~
$ ./configure
$ make
# make install
~~~

If you cloned BGPStream from GitHub, you will need to run `./autogen.sh` before
`./configure`.
Depending on your operating system, the `./autogen.sh` execution may require a few extra packages to run.
On Ubuntu, for example, you need `autogen`, `autoconf`, and `libtool` installed on the system before running the command.

For further information or support, please visit the
[BGPStream website](http://bgpstream.caida.org), or contact
bgpstream-info@caida.org.

Copyright and Open Source Software
----------------------------------

Unless otherwise specified (below or in file headers) BGPStream is Copyright The
Regents of the University of California and released under a BSD license. See
the [LICENSE](LICENSE) file for details.

### Embedded Code

Below is a list of third-party code distributed as part of the libBGPStream
package. While we make every effort to keep this list current, license
information in file headers should be considered authoritative.

 - [common/](https://github.com/caida/cc-common) - the common submodule contains
   code released under multiple licenses (BSD, MIT, LGPL). See
   https://github.com/caida/cc-common#copyright for more details. Note that
   while BGPStream currently links against all of the sub-libraries in this
   module, it does not require many of them for normal operation. At some point
   we plan to modify the BGPStream build process to only link against required
   libraries.

 - [lib/formats/libparsebgp](https://github.com/CAIDA/libparsebgp) - the
   libparsebgp submodule is released under a BSD license. See the associated
   [LICENSE](https://github.com/CAIDA/libparsebgp/blob/master/LICENSE) file for
   more information.

 - [utils/bgpstream_utils_patricia.c](lib/utils/bgpstream_utils_patricia.c) is
   released under an MIT license. See the header of the file for more details.

### External Dependencies

#### Required

 - [libwandio](https://research.wand.net.nz/software/libwandio.php) is released
   under an LGPL v3 license.

#### Optional

 - [librdkafka](https://github.com/edenhill/librdkafka) is
   [released under BSD and compatible licenses](https://github.com/edenhill/librdkafka/blob/master/LICENSES.txt).

 - [libsqlite3](https://sqlite.org) is
   [released as public domain](https://sqlite.org/copyright.html).
