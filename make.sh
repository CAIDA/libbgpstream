#!/bin/sh
#
# libbgpstream
#
# Chiara Orsini, CAIDA, UC San Diego
# chiara@caida.org
#
# Copyright (C) 2013 The Regents of the University of California.
#
# This file is part of libbgpstream.
#
# libbgpstream is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# libbgpstream is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
#


mac_sql_inc="/opt/local/include/mysql55/mysql"
mac_sql_lib="/opt/local/lib/mysql55/mysql"

thor_sql_inc="$HOME/localmysql/include"
thor_sql_lib="$HOME/localmysql/lib"

bgpa_prefix="$HOME/Projects/satc/repository/tools/bgpanalyzer/usr"
bgpa_inc="$bgpa_prefix/include"
bgpa_lib="$bgpa_prefix/lib"


if [ -d "$mac_sql_inc" ]; then
    all_includes="-I$mac_sql_inc -I$bgpa_inc"
    all_libs="-L$mac_sql_lib -L$bgpa_lib"
fi

if [ -d "$thor_sql_inc" ]; then
    all_includes="-I$thor_sql_inc -I$bgpa_inc"
    all_libs="-L$thor_sql_lib -L$bgpa_lib"
fi

./autogen.sh
./configure CPPFLAGS="$all_includes -I/usr/local/include" LDFLAGS="$all_libs -L/usr/local/lib" --prefix="$bgpa_prefix"
make
make install


