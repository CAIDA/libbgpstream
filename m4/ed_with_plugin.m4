# SYNOPSIS
#
#   ED_WITH_PLUGIN
#
# DESCRIPTION
#
#   This macro
#
# LICENSE
#
# bgpcorsaro
#
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
#
# Copyright (C) 2012 The Regents of the University of California.
#
# This file is part of bgpcorsaro.
#
# bgpcorsaro is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# bgpcorsaro is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
#
# ED_WITH_PLUGIN([plugin_name], [short_name], [CAPS_name], [enabled_by_default])
#
AC_DEFUN([ED_WITH_PLUGIN],
[
	AC_MSG_CHECKING([whether to build with $2 plugin])

if test "x$4" == xyes; then
        AC_ARG_WITH([$2],
	  [AS_HELP_STRING([--without-$2],
            [build without the $2 plugin])],
            [with_plugin_$2=$with_$2],
            [with_plugin_$2=yes])
else
        AC_ARG_WITH([$2],
	  [AS_HELP_STRING([--with-$2],
            [build with the $2 plugin])],
            [with_plugin_$2=$with_$2],
            [with_plugin_$2=no])
fi

        WITH_PLUGIN_$3=
        AS_IF([test "x$with_plugin_$2" != xno],
              [
		AC_DEFINE([WITH_PLUGIN_$3],[1],[Building with $2])
		ED_PLUGIN_INIT_ALL_ENABLED="${ED_PLUGIN_INIT_ALL_ENABLED}PLUGIN_INIT_ADD($1);"
	      ])
	AC_MSG_RESULT([$with_plugin_$2])

	AM_CONDITIONAL([WITH_PLUGIN_$3], [test "x$with_plugin_$2" != xno])
])
