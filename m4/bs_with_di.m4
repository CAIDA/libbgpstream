# SYNOPSIS
#
#   BS_WITH_DI
#
# LICENSE
#
# Copyright (C) 2015 The Regents of the University of California.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

AC_DEFUN([BS_WITH_DI],
[
	AC_MSG_CHECKING([whether to build with data interface: $2])

if test "x$4" == xyes; then
        AC_ARG_WITH([$2],
	  [AS_HELP_STRING([--without-$2],
            [build without the $2 data interface])],
            [with_di_$2=$with_$2],
            [with_di_$2=yes])
else
        AC_ARG_WITH([$2],
	  [AS_HELP_STRING([--with-$2],
            [build with the $2 data interface])],
            [with_di_$2=$with_$2],
            [with_di_$2=no])
fi

        WITH_DATA_INTERFACE_$3=
        AS_IF([test "x$with_di_$2" != xno],
              [
		AC_DEFINE([WITH_DATA_INTERFACE_$3],[1],[Building with $2])
		BS_DI_INIT_ALL_ENABLED="${BS_DI_INIT_ALL_ENABLED}DI_INIT_ADD($1);"
                bs_di_valid=yes
	      ])
	AC_MSG_RESULT([$with_di_$2])

	AM_CONDITIONAL([WITH_DATA_INTERFACE_$3], [test "x$with_di_$2" != xno])
])
