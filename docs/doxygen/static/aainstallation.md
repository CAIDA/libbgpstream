Installation {#installation}
============
[TOC]

This page is adapted from the default Autoconf INSTALL file included with the
Corsaro distribution.

Copyright (C) 1994-1996, 1999-2002, 2004-2012 Free Software Foundation,
Inc.

   Copying and distribution of this file, with or without modification,
are permitted in any medium without royalty provided the copyright
notice and this notice are preserved.  This file is offered as-is,
without warranty of any kind.

Basic Installation {#inst_basic}
==================

Briefly, the shell commands:
~~~
./configure; make; make install
~~~
should configure, build, and install Corsaro.  

   The `configure` shell script attempts to guess correct values for
various system-dependent variables used during compilation.  It uses
those values to create a `Makefile` in each directory of the package.
It may also create one or more `.h` files containing system-dependent
definitions.  Finally, it creates a shell script `config.status` that
you can run in the future to recreate the current configuration, and a
file `config.log` containing compiler output (useful mainly for
debugging `configure`).

   It can also use an optional file (typically called `config.cache` and enabled
with `-C`) that saves the results of its tests to speed up reconfiguring.
Caching is disabled by default to prevent problems with accidental use of stale
cache files.

   If you need to do unusual things to compile the package, please try
to figure out how `configure` could check whether to do them, and mail
diffs or instructions to corsaro-info@caida.org so they can
be considered for the next release.  If you are using the cache, and at
some point `config.cache` contains results you don't want to keep, you
may remove or edit it.

   The simplest way to compile Corsaro is:

  -# `cd` to the directory containing the source code and type
     `./configure` to configure the package for your system.
     Running `configure` might take a while.  While running, it prints
     some messages telling which features it is checking for.
  -# Type `make` to compile the package.
  -# Type `make install` to install the programs and any data files and
     documentation.  When installing into a prefix owned by root, it is
     recommended that the package be configured and built as a regular
     user, and only the `make install` phase executed with root
     privileges.
  -# You can remove the program binaries and object files from the
     source code directory by typing `make clean`.  To also remove the
     files that `configure' created (so you can compile the package for
     a different kind of computer), type `make distclean`.  There is
     also a `make maintainer-clean` target, but that is intended mainly
     for the package's developers.  If you use it, you may have to get
     all sorts of other programs in order to regenerate files that came
     with the distribution.
  -# You can use `make distcheck`, which can by used by developers to test that
     all other targets like `make install` and `make uninstall` work correctly.
     This target is generally not run by end users.

Compilers and Options {#inst_compilers}
=====================

   Some systems require unusual options for compilation or linking that
the `configure` script does not know about.  Run `./configure --help`
for details on some of the pertinent environment variables.

   You can give `configure` initial values for configuration parameters
by setting variables in the command line or in the environment.  Here
is an example that we use at CAIDA:

     ./configure CFLAGS=-I/usr/local/include LDFLAGS=-L/usr/local/lib

   See \ref inst_vars for more details.

Optional Features {#inst_options}
=================
   You can cause programs to be installed
with an extra prefix or suffix on their names by giving `configure` the
option:

\verbatim --program-prefix=PREFIX  \endverbatim

or 

\verbatim --program-suffix=SUFFIX \endverbatim

Corsaro supports additional options to `configure` which allow features to be
configured and plugins to be enabled or disabled, these include:

\verbatim --enable-debug \endverbatim
  - enable debugging mode
  - writes log output to STDERR as well as the log file
  
\verbatim --with-monitorname=NAME \endverbatim
  - explicitly define the monitor name
  - defaults to the system hostname as given by the `hostname` command
  
\verbatim --with-slash-eight=FIRST_OCTET \endverbatim
  - make use of optimizations for a /8 darknet
  - defaults to disabled

\verbatim --with[out]-[PLUGIN] \endverbatim
  - enable or disable (at compile time) a specific plugin
  - defaults depend on the plugin (run `configure` without specifying a plugin
    to see the default configuration).
  - run `configure` without these options to see the default configuration

Run `./configure --help` for a complete list of options available.

Compiling For Multiple Architectures {#inst_multarch}
====================================

   You can compile the package for more than one kind of computer at the
same time, by placing the object files for each architecture in their
own directory.  To do this, you can use GNU `make`.  `cd` to the
directory where you want the object files and executables to go and run
the `configure` script.  `configure` automatically checks for the
source code in the directory that `configure` is in and in `..`.  This
is known as a "VPATH" build.

   With a non-GNU `make`, it is safer to compile the package for one
architecture at a time in the source code directory.  After you have
installed the package for one architecture, use `make distclean` before
reconfiguring for another architecture.

   On MacOS X 10.5 and later systems, you can create libraries and
executables that work on multiple system types--known as "fat" or
"universal" binaries--by specifying multiple `-arch` options to the
compiler but only a single `-arch` option to the preprocessor.  Like
this:

     ./configure CC="gcc -arch i386 -arch x86_64 -arch ppc -arch ppc64" \
                 CXX="g++ -arch i386 -arch x86_64 -arch ppc -arch ppc64" \
                 CPP="gcc -E" CXXCPP="g++ -E"

   This is not guaranteed to produce working output in all cases, you
may have to build one architecture at a time and combine the results
using the `lipo` tool if you have problems.

Installation Names {#inst_names}
==================

   By default, `make install` installs the package's commands under
`/usr/local/bin`, include files under `/usr/local/include`, etc.  You
can specify an installation prefix other than `/usr/local` by giving
`configure` the option 

\verbatim --prefix=PREFIX \endverbatim

where PREFIX must be an absolute file name.

   You can specify separate installation prefixes for
architecture-specific files and architecture-independent files.  If you
pass the option 

\verbatim --exec-prefix=PREFIX \endverbatim

to `configure`, the package uses PREFIX as the prefix for installing programs
and libraries.  Documentation and other data files still use the regular prefix.

   In addition, if you use an unusual directory layout you can give
options like 

\verbatim --bindir=DIR \endverbatim

to specify different values for particular kinds of files.  Run `configure
--help` for a list of the directories you can set and what kinds of files go in
them.  In general, the default for these options is expressed in terms of
`${prefix}`, so that specifying just 

\verbatim --prefix \endverbatim 

will affect all of the other directory specifications that were not explicitly
provided.

Particular systems {#inst_systems}
==================

   On HP-UX, the default C compiler is not ANSI C compatible.  If GNU
CC is not installed, it is recommended to use the following options in
order to use an ANSI C compiler:

     ./configure CC="cc -Ae -D_XOPEN_SOURCE=500"

and if that doesn't work, install pre-built binaries of GCC for HP-UX.

   HP-UX `make` updates targets which have the same time stamps as
their prerequisites, which makes it generally unusable when shipped
generated files such as `configure` are involved.  Use GNU `make`
instead.

   On OSF/1 a.k.a. Tru64, some versions of the default C compiler cannot
parse its `<wchar.h>` header file.  The option `-nodtk` can be used as
a workaround.  If GNU CC is not installed, it is therefore recommended
to try

     ./configure CC="cc"

and if that doesn't work, try

     ./configure CC="cc -nodtk"

   On Solaris, don't put `/usr/ucb` early in your `PATH`.  This
directory contains several dysfunctional programs; working variants of
these programs are available in `/usr/bin`.  So, if you need `/usr/ucb`
in your `PATH`, put it _after_ `/usr/bin`.

   On Haiku, software installed for all users goes in `/boot/common`,
not `/usr/local`.  It is recommended to use the following options:

\verbatim ./configure --prefix=/boot/common \endverbatim

Specifying the System Type {#inst_systype}
==========================

   There may be some features `configure` cannot figure out
automatically, but needs to determine by the type of machine the package
will run on.  Usually, assuming the package is built to be run on the
_same_ architectures, `configure` can figure that out, but if it prints
a message saying it cannot guess the machine type, give it the
`--build=TYPE` option.  TYPE can either be a short name for the system
type, such as `sun4`, or a canonical name which has the form:

     CPU-COMPANY-SYSTEM

where SYSTEM can have one of these forms:

     OS
     KERNEL-OS

   See the file `config.sub` for the possible values of each field.  If
`config.sub` isn't included in this package, then this package doesn't
need to know the machine type.

   If you are _building_ compiler tools for cross-compiling, you should
use the option `--target=TYPE` to select the type of system they will
produce code for.

   If you want to _use_ a cross compiler, that generates code for a
platform different from the build platform, you should specify the
"host" platform (i.e., that on which the generated programs will
eventually be run) with `--host=TYPE`.

Sharing Defaults {#inst_defaults}
================

   If you want to set default values for `configure` scripts to share,
you can create a site shell script called `config.site` that gives
default values for variables like `CC`, `cache_file`, and `prefix`.
`configure` looks for `PREFIX/share/config.site` if it exists, then
`PREFIX/etc/config.site` if it exists.  Or, you can set the
`CONFIG_SITE` environment variable to the location of the site script.
A warning: not all `configure` scripts look for a site script.

Defining Variables {#inst_vars}
==================

   Variables not defined in a site shell script can be set in the
environment passed to `configure`.  However, some packages may run
configure again during the build, and the customized values of these
variables may be lost.  In order to avoid this problem, you should set
them in the `configure` command line, using `VAR=value`.  For example:

     ./configure CC=/usr/local2/bin/gcc

causes the specified `gcc` to be used as the C compiler (unless it is
overridden in the site shell script).

Unfortunately, this technique does not work for `CONFIG_SHELL` due to
an Autoconf limitation.  Until the limitation is lifted, you can use
this workaround:

     CONFIG_SHELL=/bin/bash ./configure CONFIG_SHELL=/bin/bash

configure Invocation {#inst_conf}
======================

   `configure` recognizes the following options to control how it
operates.

\verbatim --help (-h) \endverbatim
  - Print a summary of all of the options to `configure`, and exit.
  
\verbatim --help=short and --help=recursive \endverbatim
  - Print a summary of the options unique to this package's `configure`, and
     exit.  
  - The `short` variant lists options used only in the top level
  - The `recursive` variant lists options also present in any nested packages.
  
\verbatim --version (-V) \endverbatim
  - Print the version of Autoconf used to generate the `configure` script, and
     exit.
	 
\verbatim --cache-file=FILE \endverbatim
  - Enable the cache: use and save the results of the tests in FILE,
     traditionally `config.cache`.  
  - FILE defaults to `/dev/null` to disable
     caching.
	 
\verbatim --config-cache (-C) \endverbatim
  - Alias for `--cache-file=config.cache`.
  
\verbatim --quiet and --silent (-q) \endverbatim
  - Do not print messages saying which checks are being made.  
  - To suppress all normal output, redirect it to `/dev/null` (any error
     messages will still be shown).
	 
\verbatim --srcdir=DIR \endverbatim
  - Look for the package's source code in directory DIR.  Usually `configure`
     can determine that directory automatically.

\verbatim --prefix=DIR \endverbatim
  - Use DIR as the installation prefix.
  - See \ref inst_names or more details, including other options available for
     fine-tuning the installation locations.

\verbatim --no-create (-n) \endverbatim
  - Run the configure checks, but stop before creating any output files.
  
`configure` also accepts some other, not widely useful, options.  Run
`configure --help` for more details.

