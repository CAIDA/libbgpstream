#!/bin/sh
#

./autogen.sh
./configure --prefix="$HOME/Projects/satc/repository/tools/bgpanalyzer/usr"
make
make install

