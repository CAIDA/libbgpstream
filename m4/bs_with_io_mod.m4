# SYNOPSIS
#
#   BS_WITH_IO_MOD
#
# LICENSE
#
# This file is part of bgpstream
#
# CAIDA, UC San Diego
# bgpstream-info@caida.org
#
# Copyright (C) 2015 The Regents of the University of California.
# Authors: Alistair King, Chiara Orsini
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.
#
AC_DEFUN([BS_WITH_IO_MOD],
[
	AC_MSG_CHECKING([whether to build with BGPView IO module: $1])

if test "x$3" == xyes; then
        AC_ARG_WITH([$1-io],
	  [AS_HELP_STRING([--without-$1-io],
            [build without the $1 IO module])],
            [with_io_$1=$with_$1_io],
            [with_io_$1=yes])
else
        AC_ARG_WITH([$1-io],
	  [AS_HELP_STRING([--with-$1-io],
            [build with the $1 IO module])],
            [with_io_$1=$with_$1_io],
            [with_io_$1=no])
fi

        WITH_BGPVIEW_IO_$2=
        AS_IF([test "x$with_io_$1" != xno],
              [
		AC_DEFINE([WITH_BGPVIEW_IO_$2],[1],[Building with $1 IO])
	      ])
	AC_MSG_RESULT([$with_io_$1])

	AM_CONDITIONAL([WITH_BGPVIEW_IO_$2], [test "x$with_io_$1" != xno])
])
