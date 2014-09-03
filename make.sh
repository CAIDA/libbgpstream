#!/bin/sh
#

./autogen.sh
./configure --prefix="$HOME/Projects/satc/repository/tools/bgpanalyzer/usr" CC=gcc47 --with-wandio
make
make install

