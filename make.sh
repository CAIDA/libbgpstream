#!/bin/sh
#

./autogen.sh
./configure --prefix="$BGPTOOLS_PATH" CC=gcc47 --with-wandio
make
make install

