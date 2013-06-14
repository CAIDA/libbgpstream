#
# corsaro
# 
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
# 
# Copyright (C) 2012 The Regents of the University of California.
# 
# This file is part of corsaro.
# 
# corsaro is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# corsaro is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
# 
#  This file is a modified version of the 'Makefile.am' file included with 
#  the 'traceanon' tool in libtrace 
#	(http://research.wand.net.nz/software/libtrace.php)

AM_CPPFLAGS = 

noinst_LTLIBRARIES = libanon.la

libanon_la_SOURCES = corsaro_libanon.h corsaro_libanon.c \
	rijndael.h rijndael.c panon.h panon.c

# rijndael.c does lots of nasty casting that is going to be a nightmare to fix
# and debug - I'm going to disable this warning in the interim, as it's really
# messy and hopefully isn't actually an issue. 
libanon_la_CFLAGS = -fno-strict-aliasing $(AM_CFLAGS)
