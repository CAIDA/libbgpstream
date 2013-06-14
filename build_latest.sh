#!/bin/sh
echo "updating change log"
./generate_log.sh

echo "updating autotools files"
autoreconf -vfi && ./configure && make

echo "building distribution tarball"
make dist

echo "done!"
