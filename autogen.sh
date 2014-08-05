#!/bin/sh
#

# The --force option rebuilds the configure script regardless of its timestamp in relation to that of the file configure.ac.
# The option --install copies some missing files to the directory, including the text files COPYING and INSTALL.
# We add -I config and -I m4 so that the tools find the files that we're going to place in those subdirectories instead of in the project root.

autoreconf --force --install -I config -I m4


