Overview {#index}
========

@note Both Corsaro and this documentation are still under active development and
features will likely change between versions.  
@note Please contact corsaro-info@caida.org with any questions and/or suggestions.

Introduction        {#intro}
============

Corsaro is a software suite for performing large-scale analysis of trace
data. It was specifically designed to be used with passive traces captured by
darknets, but the overall structure is generic enough to be used with any type
of passive trace data.

Because of the scale of passive trace data, Corsaro has been designed from the
ground up to be fast and efficient, both in terms of run-time and output data
size. For more details about the design goals of Corsaro, see the
\ref goals "Goals" section. 

Corsaro allows high-speed analysis of trace data on a per-packet basis and
provides a mechanism for aggregating results based on customizable time
intervals. Trace data is read using the
[libtrace](http://research.wand.net.nz/software/libtrace.php) trace processing
library, and a high-level IO abstraction layer allows results to be
transparently written to compressed files, using threaded IO. The actual trace
analysis logic is clearly separated into a set of plugins, several of which are
shipped with Corsaro. For more information about how the pieces of Corsaro fit
together, see the \ref arch "Architecture" section.

In addition to the \ref plugins "Core Plugins" which are shipped with Corsaro,
the plugin framework makes the creation of new plugins as simple as
possible. The low overhead involved in creating a new plugin, coupled with the
efficiency and reliability of Corsaro means that it can be used both to perform
ad-hoc exploratory investigations as well as in a production context to carry
out large-scale near-realtime analysis. To learn how to create a plugin, or
perform analysis on existing Corsaro results, see the \ref tutorials "Tutorials"
section.

Corsaro can be used both as a library and as a stand-alone application for
processing any format of trace data that \a libtrace supports. The Corsaro
distribution also includes several other supporting tools for basic analysis of
Corsaro output data. For information on using the Corsaro application and the
other tools included, see the \ref tools "Tools" section.

Download            {#download}
========

The latest version of Corsaro is
[$(CORSARO_VERSION)](http://www.caida.org/tools/mesurement/corsaro/downloads/corsaro-2.0.0.tar.gz),
released on MONTH DATE YEAR.
@todo insert the date when we release

We also maintain a detailed \ref changelog.


Dependencies        {#dependencies}
============

Corsaro is written in C and should compile with any ANSI compliant C Compiler
which supports the C99 standard. Please email corsaro-info@caida.org with any
issues.

Corsaro requires [libtrace](http://research.wand.net.nz/software/libtrace.php)
version 3.0.14 or higher. (3.0.8 or higher can be used if the libwandio patch
included in the corsaro distribution is applied).

Usage               {#usage}
=====

Corsaro has many different usage scenarios which are outlined in this manual,
but if you are looking to just run the analysis engine with the bundled plugins,
see the \ref tool_corsaro "Corsaro" section of the \ref tools "Tools" page.

Quick Start               {#qstart}
===========

If you want to just dive right in and get started using Corsaro, take a look at
the \ref quickstart "Quick Start" guide.

Documentation       {#docs}
=============

The online
[Corsaro Documentation](http://www.caida.org/tools/measurement/corsaro/docs/) is
the best source of information about using Corsaro. It contains full API
documentation, usage instructions for the Corsaro tools. It also has tutorials
about writing Corsaro plugins and using the libcorsaro library to perform
analysis on Corsaro-generated data.
